#include "peripherals.h"
#include "optical.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include <freertos/ringbuf.h>

namespace rift {
namespace {
constexpr rmt_channel_t tx=RMT_CHANNEL_0, rx=RMT_CHANNEL_4;
RingbufHandle_t ring=nullptr;
constexpr uint16_t tunes[][9]={
    {740,988,0}, {392,523,784,0}, {1397,1047,784,0}, {294,440,588,0},
    {659,784,988,0}, {392,247,165,0}, {523,659,784,1047,784,988,1047,0},
    {220,330,294,440,0}
};
}

bool Peripherals::initIR() {
    M5.Power.setExtOutput(true);
    rmt_config_t t=RMT_DEFAULT_CONFIG_TX(GPIO_NUM_46,tx);
    t.clk_div=80;
    t.tx_config.carrier_en=true;
    t.tx_config.carrier_freq_hz=38000;
    t.tx_config.carrier_duty_percent=33;
    if (rmt_config(&t)!=ESP_OK || rmt_driver_install(tx,0,0)!=ESP_OK) return false;
    rmt_config_t r=RMT_DEFAULT_CONFIG_RX(GPIO_NUM_42,rx);
    r.clk_div=80;
    r.mem_block_num=2;
    r.rx_config.filter_ticks_thresh=80;
    r.rx_config.idle_threshold=12000;
    if (rmt_config(&r)!=ESP_OK || rmt_driver_install(rx,2048,0)!=ESP_OK
        || rmt_get_ringbuf_handle(rx,&ring)!=ESP_OK || !ring) return false;
    gpio_pullup_en(GPIO_NUM_42);
    rmt_rx_stop(rx);
    infrared=true;
    return true;
}

bool Peripherals::listen() {
    if (receiving) return true;
    quiet();
    if (!infrared) return false;
    size_t n;
    while (void *old=xRingbufferReceive(ring,&n,0)) vRingbufferReturnItem(ring,old);
    receiving=rmt_rx_start(rx,true)==ESP_OK;
    return receiving;
}
void Peripherals::stopListening() {
    if (receiving) rmt_rx_stop(rx);
    receiving=false;
}

IRRead Peripherals::readIR(uint32_t &word, Print *trace) {
    if (!receiving) return IRRead::Empty;
    size_t size=0;
    auto items=static_cast<rmt_item32_t*>(xRingbufferReceive(ring,&size,0));
    if (!items) return IRRead::Empty;
    if (trace) {
        trace->print("RIFT PULSES");
        for (size_t i=0;i<size/sizeof(*items);++i) {
            trace->printf(" %d,%d",items[i].level0 ? int(items[i].duration0) : -int(items[i].duration0),
                          items[i].level1 ? int(items[i].duration1) : -int(items[i].duration1));
        }
        trace->println();
    }
    // The legacy RMT driver can deliver more than one hardware memory block.
    // Skip noise before the NEC header and leave room for split marks.
    Pulse frame[72]={};
    size_t marks=0;
    bool malformed=false;
    for (size_t i=0;i<size/sizeof(*items);++i) {
        for (int half=0;half<2;++half) {
            auto level=half ? items[i].level1 : items[i].level0;
            auto us=half ? items[i].duration1 : items[i].duration0;
            if (!us) continue;
            if (!level) {
                if (!marks && !nearPulse(us,9000)) continue;
                if (marks==sizeof(frame)/sizeof(frame[0])) break;
                if (marks && !frame[marks-1].space) { malformed=true; break; }
                frame[marks++].mark=us;
            } else if (marks) frame[marks-1].space=us;
        }
        if (malformed || marks==sizeof(frame)/sizeof(frame[0])) break;
    }
    vRingbufferReturnItem(ring,items);
    return !malformed && readCapture(frame,marks,word) ? IRRead::Decoded : IRRead::Malformed;
}
bool Peripherals::sendIR(uint32_t word) {
    if (!infrared || receiving) return false;
    Pulse pulses[34];
    rmt_item32_t items[34]={};
    makeFrame(word,pulses);
    for (int i=0;i<34;++i) {
        items[i].level0=1; items[i].duration0=pulses[i].mark;
        items[i].level1=0; items[i].duration1=pulses[i].space;
    }
    return rmt_write_items(tx,items,34,true)==ESP_OK;
}
bool Peripherals::sendShot(uint32_t word) {
    if (!infrared || receiving) return false;
    // Retain hardware items until TX completion. Gameplay keeps sampling
    // buttons and motion while the carrier is active.
    static rmt_item32_t items[shotParts*shardPulses];
    static bool started=false;
    if (started && rmt_wait_tx_done(tx,0)!=ESP_OK) return false;
    Pulse pulses[shotParts*shardPulses];
    makeShot(word,pulses);
    for (unsigned i=0;i<shotParts*shardPulses;++i) {
        items[i].level0=1; items[i].duration0=pulses[i].mark;
        items[i].level1=0; items[i].duration1=pulses[i].space;
    }
    started=rmt_write_items(tx,items,shotParts*shardPulses,false)==ESP_OK;
    return started;
}
bool Peripherals::readShot(uint32_t word,uint8_t &mask) {
    if (!receiving) return false;
    size_t size=0;
    auto items=static_cast<rmt_item32_t*>(xRingbufferReceive(ring,&size,0));
    if (!items) return false;
    uint32_t edges[96], starts[96], elapsed=0;
    size_t count=0;
    for (size_t i=0;i<size/sizeof(*items);++i) {
        for (int half=0;half<2;++half) {
            unsigned duration=half ? items[i].duration1 : items[i].duration0;
            unsigned level=half ? items[i].level1 : items[i].level0;
            elapsed+=duration;
            if (!level && duration>=100 && count<96) {
                starts[count]=elapsed-duration; edges[count++]=elapsed;
            }
        }
    }
    vRingbufferReturnItem(ring,items);
    mask|=matchShotEdges(edges,count,word) | matchShotEdges(starts,count,word);
    return true;
}
void Peripherals::quiet() {
    notes=nullptr;
    if (microphone) M5.Mic.end();
    if (!amplifierQuiet) {
        M5.Speaker.end(); // Disable PYG3, including the first receive window after boot.
        amplifierQuiet=true;
    }
    microphone=recording=speaker=false;
}
void Peripherals::startVoice() {
    if (receiving) return;
    quiet();
    microphone=M5.Mic.begin();
}
void Peripherals::updateVoice(Voice &voice) {
    if (!microphone || M5.Mic.isRecording()) return;
    if (recording) {
        int32_t sum=0, amplitude=0;
        for (auto s : samples) sum+=s;
        int mean=sum/320;
        for (auto s : samples) amplitude+=abs(s-mean);
        voice.feed(amplitude/320>360,20);
    }
    recording=M5.Mic.record(samples,320,16000);
}
void Peripherals::play(Tune which) {
    if (receiving) return;
    if (microphone) { M5.Mic.end(); microphone=recording=false; }
    if (!speaker) {
        amplifierQuiet=false;
        speaker=M5.Speaker.begin(); audioOn=millis();
        M5.Speaker.setVolume(72);
    }
    notes=tunes[which]; note=0; lastNote=millis()-150;
}
void Peripherals::updateAudio(uint32_t now) {
    if (!speaker || !notes || now-audioOn<1000 || now-lastNote<145) return;
    uint16_t freq=notes[note++];
    if (!freq) { notes=nullptr; return; }
    M5.Speaker.tone(freq,118); lastNote=now;
}
}

