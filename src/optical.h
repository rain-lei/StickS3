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
inline bool bitMark(int us) { return us>=300 && us<=800; }
inline bool readFrame(const Pulse *in, size_t n, uint32_t &word) {
    if (n!=34 || !nearPulse(in[0].mark,9000) || !nearPulse(in[0].space,4500)
        || !bitMark(in[33].mark)) return false;
    uint32_t value=0;
    for (unsigned i=0;i<32;++i) {
        const Pulse &bit=in[i+1];
        if (!bitMark(bit.mark) || bit.space<250) return false;
        // Demodulation can extend the mark into its space. Leading-edge
        // spacing still distinguishes a zero (~1120 us) from a one (~2250 us).
        int period=bit.mark+bit.space;
        bool nominalMark=nearPulse(bit.mark,560);
        if ((nominalMark && nearPulse(bit.space,1690)) || (period>=1780 && period<=2700))
            value |= 1u<<i;
        else if (!(nominalMark && nearPulse(bit.space,560)) && (period<850 || period>1450))
            return false;
    }
    word=value;
    return true;
}
inline bool readCapture(const Pulse *in,size_t count,uint32_t &word) {
    for (size_t start=0; start+34<=count; ++start) {
        if (readFrame(in+start,34,word)) return true;
    }
    return false;
}
}

