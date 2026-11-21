#pragma once
#include <stdint.h>
#include <math.h>
#include <algorithm>

namespace rift {
enum Skill : uint8_t { Flame, Frost, Ward, Bloom };
enum Mode : uint8_t { Trial, Duel, Raid };
enum Effect : uint8_t { Waiting, Burn, Freeze, Block, Miss, Guard, Heal };
inline int clamp(int v, int low, int high) { return std::max(low, std::min(high, v)); }

// Acceleration in g, angular speed about the wand's long axis in degrees/s.
struct Gesture {
    float x0, y0, z0, lastAngle, winding, roll;
    bool tracking, axialImpulse, seen[4];
    void start(float x, float y, float z) {
        x0=x; y0=y; z0=z;
        tracking=axialImpulse=false; winding=roll=lastAngle=0;
        for (bool &v : seen) v=false;
    }
    void sample(float x, float y, float z, float gyroY, unsigned ms) {
        float dx=x-x0, dy=y-y0;
        if (fabsf(dx)>0.92f && fabsf(dx)>1.3f*fabsf(dy)) seen[Flame]=true;
        if (!axialImpulse && fabsf(dy)>0.92f && fabsf(dy)>1.3f*fabsf(dx)) {
            axialImpulse=true;
            seen[Frost]=dy>0;
        }
        roll += gyroY * std::min(ms, 50u) / 1000.f;
        if (fabsf(roll)>44) seen[Ward]=true;
        if (dx*dx+dy*dy < 0.09f || fabsf(z-z0)>1.6f) {
            tracking=false; winding=0;
            return;
        }
        float angle=atan2f(dy, dx);
        if (tracking) {
            float delta=angle-lastAngle;
            if (delta>3.14159f) delta-=6.28318f;
            if (delta< -3.14159f) delta+=6.28318f;
            winding = fabsf(delta)<1.15f ? winding+delta : 0;
            if (fabsf(winding)>5.25f) seen[Bloom]=true;
        }
        tracking=true; lastAngle=angle;
    }
    bool matches(Skill skill) const { return seen[skill]; }
};

struct Voice {
    unsigned voiced=0, quiet=90, beats=0;
    void feed(bool loud, unsigned ms) {
        if (loud) {
            if (quiet>=55) ++beats;
            quiet=0;
            voiced+=ms;
        } else quiet+=ms;
    }
    int energy() const { return clamp(voiced/18 + 4*std::min(beats, 5u), 0, 65); }
};

struct Battle {
    Mode mode;
    int hp[2]={100,100}, boss, turn=0;
    bool shield[2]={false,false};
    Skill stance[2]={Flame,Flame};
    Effect effect=Waiting;
    explicit Battle(Mode kind=Trial) : mode(kind), boss(kind==Raid ? 145 : 100) {}
    int actor() const { return mode==Trial ? 0 : turn%2; }
    bool finished() const { return hp[0]==0 || hp[1]==0 || (mode!=Duel && boss==0); }
    Skill omen() const { return (turn/2)%2 ? Frost : Flame; }
    void resolve(Skill skill, int energy, bool hit, bool reflex) {
        if (finished()) return;
        int a=actor(), b=1-a;
        int power=15+clamp(energy,0,100)/7;
        stance[a]=skill;
        if (skill==Bloom) {
            hp[a]=clamp(hp[a]+power,0,100);
            if (mode==Raid) hp[b]=clamp(hp[b]+power/2,0,100);
            effect=Heal;
        } else if (skill==Ward) {
            shield[a]=true; effect=Guard;
        } else if (mode!=Duel) {
            boss=clamp(boss-power-(skill!=omen() ? 7 : 0),0,145);
            effect=skill==Frost ? Freeze : Burn;
        } else if (!hit) effect=Miss;
        else if (reflex || shield[b]) {
            hp[a]=clamp(hp[a]-7,0,100);
            shield[b]=false; effect=Block;
        } else {
            int bonus=(skill==Frost && stance[b]==Flame) || (skill==Flame && stance[b]==Bloom) ? 6 : 0;
            hp[b]=clamp(hp[b]-power-bonus,0,100);
            effect=skill==Frost ? Freeze : Burn;
        }
        if (mode!=Duel && boss>0) {
            hp[a]=clamp(hp[a]-(shield[a] ? 3 : 10+turn%4*2),0,100);
            shield[a]=false;
        }
        ++turn;
    }
};
}

