#pragma once
#include "pet.h"
#include <RiftBoardUi.h>
namespace rift {
enum Scene { Nest, Training, Solo, Hosting, Search, Online, Runes, Learn, Invoke, Challenge, Reward, LinkRune };
struct View {
    Scene scene=Nest;
    ui::BoardView status;
    const char *title="", *skillName="";
    bool hasNotice=false;
    bool asleep=false, charging=false, evolved=false, evolving=false, onBeat=false;
    bool incoming=false, rivalDragon=false;
    int energy=0, level=1, levelXp=0, mastery=0, hp=100, enemy=-1, ally=-1, enemyMax=100;
    Mode roomMode=Duel;
    Skill skill=Flame;
    Effect effect=Waiting;
    uint32_t fxAge=9999;
};
void initDisplay();
void draw(const View &v,uint32_t now);
}

