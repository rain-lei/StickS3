#pragma once
#include <stdint.h>
#include "rules.h"
class Print;

namespace rift {
enum Tune { Chirp, FireTone, IceTone, ShieldTone, HealTone, HurtTone, VictoryTone, BattleTone };
enum class IRRead { Empty, Malformed, Decoded };
class Peripherals {
public:
    bool initIR();
    bool listen();
    void stopListening();
    IRRead readIR(uint32_t &word, Print *trace=nullptr);
    bool sendIR(uint32_t word);
    void startVoice();
    void updateVoice(Voice &v);
    void quiet();
    void play(Tune tune);
    void updateAudio(uint32_t now);
private:
    bool infrared=false, receiving=false, microphone=false, speaker=false, recording=false;
    int16_t samples[320]={};
    const uint16_t *notes=nullptr;
    int note=0;
    uint32_t audioOn=0,lastNote=0;
};
}

