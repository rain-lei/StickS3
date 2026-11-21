#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

namespace rift {
struct Pulse { uint16_t mark, space; };
inline uint32_t token(uint32_t room, uint32_t turn) {
    uint32_t mix=(room ^ (turn*0x9e3779b9u))*0x85ebca6bu;
    mix ^= mix>>16;
    return (mix & 0xffffffu) | ((mix & 0xff0000u)<<8);
}
inline uint32_t shotToken(uint32_t room,uint32_t seq,bool reverse) {
    // Keep the echo domain distinct even if the same emitter sees its own light.
    return token(room,seq) ^ (reverse ? 0x96966db3u : 0u);
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
        if (!nearPulse(in[start].mark,9000) || !nearPulse(in[start].space,4500)) continue;
        // Use the ends of demodulated marks. A late falling edge then
        // transfers time between a space and the next mark without changing
        // their sum. Only bridge short interruptions inside one NEC mark.
        size_t pos=start+1;
        uint32_t value=0;
        int previousSpace=0;
        for (unsigned symbol=0; symbol<=32 && pos<count; ++symbol) {
            unsigned mark=in[pos].mark;
            while (pos+1<count && in[pos].space>0 && in[pos].space<=220
                   && mark+in[pos].space+in[pos+1].mark<=950) {
                mark+=in[pos].space+in[pos+1].mark;
                ++pos;
            }
            if (mark<150 || mark>950) break;
            if (symbol) {
                int period=previousSpace+mark;
                if (period>=1780 && period<=2700) value|=1u<<(symbol-1);
                else if (period<850 || period>1450) break;
            }
            if (symbol==32) { word=value; return true; }
            previousSpace=in[pos++].space;
            if (previousSpace<250) break;
            // A brief false low can interrupt a data space. It is too short
            // to be a complete mark and cannot supply a missing data bit.
            while (pos+1<count && in[pos].mark<150 && in[pos].space>220
                   && previousSpace+in[pos].mark+in[pos].space<=2400) {
                previousSpace+=in[pos].mark+in[pos].space;
                ++pos;
            }
        }
    }
    return false;
}

// Three equal bursts form an independently identifiable nibble. Measuring
// either all rising or all falling edges cancels a consistent demodulator lag.
constexpr unsigned shotParts=8, shardPulses=3;
constexpr uint8_t allShotParts=0xff;
inline int indexPeriod(unsigned index) { return 2600+1000*index; }
inline int nibblePeriod(unsigned value) { return 2600+680*value; }
// token() has 24 independent bits. Two GF(16) parity symbols make any six
// distinct indexed symbols sufficient to recover those bits (two erasures).
inline unsigned shotSymbol(uint32_t word,unsigned index) {
    if (index<6) return (word>>(4*index))&15;
    unsigned parity=0;
    for (int i=5;i>=0;--i) {
        if (index==7) parity=((parity<<1)^((parity&8) ? 0x13 : 0))&15;
        parity^=(word>>(4*i))&15;
    }
    return parity;
}
inline bool completeShot(uint8_t mask) {
    unsigned parts=0;
    for (;mask;mask&=mask-1) ++parts;
    return parts>=6;
}
inline void makeShot(uint32_t word,Pulse *out) {
    constexpr uint16_t mark=1400;
    for (unsigned i=0;i<shotParts;++i) {
        unsigned nibble=shotSymbol(word,i);
        Pulse *part=out+i*shardPulses;
        part[0]={mark,uint16_t(indexPeriod(i)-mark)};
        part[1]={mark,uint16_t(nibblePeriod(nibble)-mark)};
        part[2]={mark,uint16_t(i+1==shotParts ? 0 : 15000)};
    }
}
inline uint8_t matchShotEdges(const uint32_t *edges,size_t count,uint32_t word) {
    // RMT's idle boundary separates nibbles. A weak mark may be split, so
    // try extra edges but require both intervals in the same capture.
    if (count<3 || count>16) return 0;
    uint8_t mask=0;
    for (size_t first=0;first+2<count;++first) {
        if (edges[first]-edges[0]>1600) break;
        for (size_t second=first+1;second+1<count;++second) {
            int interval=edges[second]-edges[first];
            if (interval>indexPeriod(7)+300) break;
            if (interval<indexPeriod(0)-300) continue;
            unsigned index=(interval-indexPeriod(0)+500)/1000;
            if (abs(interval-indexPeriod(index))>300) continue;
            int period=nibblePeriod(shotSymbol(word,index));
            for (size_t third=second+1;third<count;++third) {
                int gap=edges[third]-edges[second];
                if (gap>period+270) break;
                if (edges[count-1]-edges[third]>1600) continue;
                if (gap>=period-270 && abs(int(edges[third]-edges[first])-indexPeriod(index)-period)<=330) {
                    mask|=1u<<index; break;
                }
            }
        }
    }
    return mask;
}
}
