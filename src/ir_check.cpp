#include "ir_check.h"
#include "optical.h"
#include <Arduino.h>
#include <cstring>
#include <cstdlib>

namespace rift {
namespace {
bool listening=false;
uint32_t started=0;
unsigned traced=0;
char line[40]={};
size_t used=0;
bool overflow=false;
void stopIRCheck(Peripherals &device) {
    if (listening) device.stopListening();
    listening=false;
    used=0;
    overflow=false;
}
}
bool pollIRCheck(Peripherals &device) {
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
        if (!strcmp(line,"RIFT PING")) Serial.println("RIFT NEST");
        else if (!strcmp(line,"RIFT RX")) {
            stopIRCheck(device);
            listening=device.listen();
            started=millis(); traced=0;
            Serial.println(listening ? "RIFT READY" : "RIFT RX ERROR");
        } else if (!strcmp(line,"RIFT STOP")) {
            stopIRCheck(device); Serial.println("RIFT STOPPED");
        } else if (!strncmp(line,"RIFT TX ",8)) {
            char *end=nullptr;
            long index=strtol(line+8,&end,10);
            if (end==line+8 || *end || index<0 || index>65535) continue;
            stopIRCheck(device); device.quiet();
            uint32_t code=token(0x52494654,uint16_t(index));
            if (device.sendIR(code)) Serial.printf("RIFT SENT %08lX\n",(unsigned long)code);
            else Serial.println("RIFT TX ERROR");
        }
    }
    if (listening) {
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
