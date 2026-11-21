#include "../src/rules.h"
#include "../src/optical.h"
#include "../src/pet.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace rift;

int main() {
    Gesture g;
    g.start(0, 0, 1);
    for (int i = 0; i < 60; ++i) g.sample(0, 0, 1, 0, 10);
    assert(!g.matches(Frost) && !g.matches(Flame) && !g.matches(Ward) && !g.matches(Bloom));
    for (int i = 0; i <= 60; ++i) {
        float a = i * 0.106f;
        g.sample(0.6f * cosf(a), 0.6f * sinf(a), 1, 0, 12);
    }
    assert(g.matches(Bloom));
    g.sample(0, 1.3f, 1, 0, 12);
    assert(g.matches(Frost));
    g.start(0, 0, 1);
    g.sample(0, -1.3f, 1, 0, 12);
    g.sample(0, 1.2f, 1, 0, 12);
    assert(!g.matches(Frost)); // braking after a backward pull
    g.start(0, 0, 1);
    g.sample(1.35f, 0, 1, 0, 12);
    assert(g.matches(Flame) && !g.matches(Frost));
    for (int i = 0; i < 30; ++i) g.sample(0, 0, 1, 210, 10);
    assert(g.matches(Ward));
    Voice sustained, rhythmic;
    for (int i = 0; i < 40; ++i) {
        sustained.feed(true, 20);
        rhythmic.feed(i % 12 < 9, 20);
    }
    assert(sustained.energy() > 0 && rhythmic.beats > sustained.beats);

    Battle raid(Raid);
    raid.hp[1] = 30;
    raid.resolve(Bloom, 50);
    assert(raid.hp[1] > 30 && raid.turn == 1);
    while (!raid.finished()) raid.resolve(Flame, 100);
    assert(raid.boss == 0);
    auto turn = raid.turn;
    raid.resolve(Flame, 100);
    assert(raid.turn == turn);
    Battle solo(Trial);
    while (!solo.finished()) solo.resolve(Ward, 0);
    assert(solo.turn > 0 && solo.actor() == 0);

    uint32_t word = 0xBA45FF00;
    Pulse pulses[34];
    makeFrame(word, pulses);
    uint32_t decoded = 0;
    assert(readFrame(pulses, 34, decoded) && decoded == word && remoteCode(word));
    assert(!remoteCode(token(12345, 7)));
    pulses[6].space = 3100;
    assert(!readFrame(pulses, 34, decoded));
    assert(!readFrame(pulses, 2, decoded));
    uint32_t extended=0xB54A3812;
    makeFrame(extended,pulses);
    assert(readFrame(pulses,34,decoded) && remoteCode(decoded) && decoded==extended);
    makeFrame(token(9,3),pulses);
    assert(readFrame(pulses,34,decoded) && decoded!=token(9,4) && !remoteCode(decoded));
    Pulse captured[36]={{90,180},{120,600}};
    makeFrame(token(9,3),captured+2);
    for (int i=3;i<35;++i) {
        captured[i].mark+=210;
        captured[i].space-=210;
    }
    assert(readCapture(captured,36,decoded) && decoded==token(9,3));
    assert(!readCapture(captured,35,decoded));
    captured[9].space=3100;
    assert(!readCapture(captured,36,decoded));
    // COM6 -> COM8. Leading-edge delays shortened individual marks while
    // the intervals between successive ends still carry all 32 data bits.
    Pulse bench[]={{9046,4742},{307,535},{623,1640},{621,1983},{257,480},{598,666},
        {518,1822},{418,478},{595,589},{595,536},{516,642},{465,1824},{493,480},{657,555},
        {574,531},{580,1656},{572,1612},{645,619},{437,1852},{465,534},{624,1664},{546,1613},
        {649,1640},{600,478},{650,562},{563,594},{522,1687},{548,479},{650,1692},{595,1563},
        {656,1790},{441,480},{644,594},{544,0}};
    assert(readCapture(bench,34,decoded) && decoded==0x3A3AC426);
    Pulse split[35];
    std::copy(bench,bench+3,split);
    split[3]={84,152}; split[4]={385,bench[3].space};
    std::copy(bench+4,bench+34,split+5);
    assert(readCapture(split,35,decoded) && decoded==0x3A3AC426);
    assert(!readCapture(split,34,decoded));
    Pet dragon;
    for(int i=0;i<4;++i) dragon.train(Frost);
    assert(dragon.trained(Frost) && dragon.practice[Frost]==3 && !dragon.trained(Ward));
    dragon.reward(true);
    dragon.reward(false);
    assert(dragon.level()==2 && dragon.runes==1 && dragon.wins==2);
    // Independent fragments can complete one shot over retries. An index,
    // room token or different payload cannot substitute for a missing byte.
    Pulse shot[shotParts*shardPulses];
    uint32_t signature=0x76543210, restored=0;
    makeShot(signature,shot);
    for (unsigned i=0;i<shotParts;++i) {
        uint32_t ends[shardPulses], elapsed=0;
        for (unsigned j=0;j<shardPulses;++j) {
            Pulse part=shot[i*shardPulses+j];
            elapsed+=part.mark;
            ends[j]=elapsed+(j%2 ? 110 : 0);
            elapsed+=part.space;
        }
        auto mask=matchShotEdges(ends,shardPulses,signature);
        assert(mask==(1<<i));
        restored|=mask;
        assert(!matchShotEdges(ends,shardPulses-1,signature));
        assert(!matchShotEdges(ends,shardPulses,signature^(1u<<(4*(i%6)))));
    }
    assert(shotToken(17,1,false)!=shotToken(17,1,true));
    assert(restored==allShotParts && completeShot(0xf3) && !completeShot(0x1f));
    // Any changed data nibble changes both parities. Any two changed
    // data nibbles must change at least one parity.
    for (int a=0;a<6;++a) for (uint32_t d=1;d<16;++d) {
        uint32_t one=signature^(d<<(4*a));
        assert(shotSymbol(one,6)!=shotSymbol(signature,6) && shotSymbol(one,7)!=shotSymbol(signature,7));
        for (int b=a+1;b<6;++b) for (uint32_t e=1;e<16;++e) {
            uint32_t two=one^(e<<(4*b));
            assert(shotSymbol(two,6)!=shotSymbol(signature,6) || shotSymbol(two,7)!=shotSymbol(signature,7));
        }
    }
    puts("PASS gestures, voice rhythm, projectile fragments, co-op, boss, NEC, game frames and progression");
}

