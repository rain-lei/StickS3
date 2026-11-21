#pragma once
#include "rules.h"
namespace rift {
struct Pet {
    uint32_t version=1, xp=0, wins=0, runes=0;
    uint8_t practice[4]={};
    int level() const { return 1+std::min(xp/30,29u); }
    bool trained(Skill s) const { return practice[s]>=3; }
    void train(Skill s) { if (practice[s]<3) ++practice[s]; xp+=2; }
    void reward(bool boss) { ++wins; xp+=boss ? 22 : 12; if (boss) ++runes; }
};
}

