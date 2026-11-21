#pragma once
#include "pet.h"
namespace rift {
enum Scene { Nest, Training, Solo, Hosting, Search, Online, Runes, Learn, Invoke, Challenge, Reward, LinkRune };
struct View {
    Scene scene=Nest;
    const char *title="", *action="", *hint="", *notice="";
    bool asleep=false, charging=false, evolved=false, evolving=false, onBeat=false;
    bool incoming=false, rivalDragon=false;
    int energy=0, level=1, hp=100, enemy=-1, ally=-1, enemyMax=100;
    Skill skill=Flame;
    Effect effect=Waiting;
    uint32_t fxAge=9999;
};
void initDisplay();
void draw(const View &v,uint32_t now);
}

