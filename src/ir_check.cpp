#include "ir_check.h"
#include "optical.h"
#include <Arduino.h>
#include <cstring>
#include <cstdlib>

namespace rift {
namespace {
bool watching=false;
uint32_t watchAt=0;
bool listening=false;
uint32_t started=0;
unsigned traced=0;
bool game=false;
uint32_t expected=0;
uint8_t mask=0;
char line[40]={};
size_t used=0;
bool overflow=false;
bool diagnosticWord(const char *arg,uint32_t &word) {
    char *end=nullptr;
    long index=strtol(arg,&end,10);
    bool reverse=!strcmp(end," R");
    if (end==arg || index<0 || index>65535 || (*end && !reverse)) return false;
    word=shotToken(0x52494654,uint32_t(index),reverse);
    return true;
}
void stopIRCheck(Peripherals &device) {
    if (listening) device.stopListening();
    listening=false;
    used=0;
    overflow=false;
}
}
bool watchEnabled(uint32_t now) { return watching && now-watchAt<3500; }
bool pollIRCheck(Peripherals &device,bool nest) {
    if (listening && millis()-started>=3200) {
        stopIRCheck(device);
        Serial.println("RIFT END");
    }
    while (Serial.available()) {
        int ch=Serial.read();
        if (ch=='\r') continue;
        if (ch!='\n') {
            if (used<sizeof(line)-1) line[used++]=char(ch);
            else overflow=true;
            continue;
        }
        line[used]=0;
        used=0;
        if (overflow) { overflow=false; continue; }
        if (!strcmp(line,"RIFT WATCH")) { watching=true; watchAt=millis(); continue; }
        if (!strcmp(line,"RIFT UNWATCH")) { watching=false; continue; }
        if (!nest) continue;
        if (!strcmp(line,"RIFT PING")) Serial.println("RIFT NEST");
        else if (!strcmp(line,"RIFT RX") || !strncmp(line,"RIFT ARM ",9)) {
            stopIRCheck(device);
            game=!strncmp(line,"RIFT ARM ",9);
            mask=0;
            if (game && !diagnosticWord(line+9,expected)) continue;
            listening=device.listen();
            started=millis(); traced=0;
            Serial.println(listening ? "RIFT READY" : "RIFT RX ERROR");
        } else if (!strcmp(line,"RIFT STOP")) {
            stopIRCheck(device); Serial.println("RIFT STOPPED");
        } else if (!strncmp(line,"RIFT TX ",8) || !strncmp(line,"RIFT SH ",8)) {
            uint32_t code;
            if (!diagnosticWord(line+8,code)) continue;
            stopIRCheck(device); device.quiet();
            if (line[5]=='S' ? device.sendShot(code) : device.sendIR(code)) Serial.printf("RIFT SENT %08lX\n",(unsigned long)code);
            else Serial.println("RIFT TX ERROR");
        }
    }
    if (listening && game) {
        uint8_t before=mask;
        while (device.readShot(expected,mask)) {}
        if (mask!=before) Serial.printf("RIFT PARTS %X\n",mask);
        if (completeShot(mask) && !completeShot(before)) Serial.printf("RIFT WORD %08lX\n",(unsigned long)expected);
    } else if (listening) {
        uint32_t got=0;
        auto result=device.readIR(got,traced<2 ? &Serial : nullptr);
        if (result!=IRRead::Empty) {
            ++traced;
            if (result==IRRead::Decoded) Serial.printf("RIFT WORD %08lX\n",(unsigned long)got);
            else Serial.println("RIFT MALFORMED");
        }
    }
    return listening;
}
}
