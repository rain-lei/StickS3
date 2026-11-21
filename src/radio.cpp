#include "radio.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace rift { namespace radio {
namespace {
QueueHandle_t inbox=nullptr;
const uint8_t broadcast[6]={255,255,255,255,255,255};
uint8_t peer[6]={};
bool running=false, bound=false;
void onReceive(const uint8_t *mac,const uint8_t *data,int len) {
    if (!inbox || len!=sizeof(Packet)) return;
    Envelope in;
    memcpy(in.mac,mac,6);
    memcpy(&in.data,data,sizeof(Packet));
    if (valid(in.data)) xQueueSend(inbox,&in,0);
}
bool add(const uint8_t *address) {
    if (esp_now_is_peer_exist(address)) return true;
    esp_now_peer_info_t info={};
    memcpy(info.peer_addr,address,6); info.channel=1; info.ifidx=WIFI_IF_STA;
    return esp_now_add_peer(&info)==ESP_OK;
}
}
bool begin() {
    if (running) return true;
    if (!inbox) inbox=xQueueCreate(12,sizeof(Envelope));
    if (!inbox) return false;
    xQueueReset(inbox);
    WiFi.persistent(false);
    if (!WiFi.mode(WIFI_STA)) return false;
    WiFi.disconnect(false,false);
    WiFi.setSleep(false);
    if (esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE)!=ESP_OK || esp_now_init()!=ESP_OK) {
        WiFi.mode(WIFI_OFF); return false;
    }
    running=true;
    if (esp_now_register_recv_cb(onReceive)!=ESP_OK || !add(broadcast)) { end(); return false; }
    return true;
}
void end() {
    if (running) {
        esp_now_unregister_recv_cb(); esp_now_deinit(); WiFi.mode(WIFI_OFF);
    }
    running=bound=false;
}
bool bind(const uint8_t *address) {
    if (!running || !add(address)) return false;
    memcpy(peer,address,6); bound=true; return true;
}
bool fromPeer(const Envelope &in) { return bound && !memcmp(peer,in.mac,6); }
bool next(Envelope &in) { return running && xQueueReceive(inbox,&in,0)==pdTRUE; }
void send(const Packet &p,bool advertise) {
    if (running && (advertise || bound))
        esp_now_send(advertise ? broadcast : peer,(const uint8_t*)&p,sizeof(p));
}
}}

