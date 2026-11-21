#pragma once
#include <stdint.h>
#include <stddef.h>

namespace rift {
struct Pulse { uint16_t mark, space; };
inline uint32_t token(uint32_t room, uint16_t turn) {
    uint32_t mix=(room ^ (turn*0x9e3779b9u))*0x85ebca6bu;
    mix ^= mix>>16;
    return (mix & 0xffffffu) | ((mix & 0xff0000u)<<8);
}
inline bool remoteCode(uint32_t word) {
    return uint8_t((word>>16) ^ (word>>24)) == 0xff;
}
inline void makeFrame(uint32_t word, Pulse *out) {
    out[0]={9000,4500};
    for (unsigned i=0;i<32;++i)
        out[i+1]={560, uint16_t((word & (1u<<i)) ? 1690 : 560)};
    out[33]={560,0};
}
inline bool nearPulse(int actual, int ideal) { return actual>=ideal*65/100 && actual<=ideal*135/100; }
inline bool readFrame(const Pulse *in, size_t n, uint32_t &word) {
    if (n!=34 || !nearPulse(in[0].mark,9000) || !nearPulse(in[0].space,4500)
        || !nearPulse(in[33].mark,560)) return false;
    uint32_t value=0;
    for (unsigned i=0;i<32;++i) {
        if (!nearPulse(in[i+1].mark,560)) return false;
        if (nearPulse(in[i+1].space,1690)) value |= 1u<<i;
        else if (!nearPulse(in[i+1].space,560)) return false;
    }
    word=value;
    return true;
}
}

