#pragma once
#include "duel.h"

namespace rift {
enum Phase : uint8_t { Lobby, Prepare, Feedback };
enum Signal : uint8_t { Idle, Cast, Leaving };

// Both devices run the same little-endian firmware. Fixed width wire fields only.
struct Packet {
    uint32_t magic=0x32544652, room=0, client=0, revision=0;
    uint32_t turn=0;
    uint8_t host=0, mode=0, phase=0, signal=0, skill=0, energy=0;
    uint8_t hp0=100,hp1=100,boss=0,effect=0;
    DuelWire duel;
};
static_assert(sizeof(Packet)<=250,"Unexpected room packet layout");
inline bool valid(const Packet &p) {
    return p.magic==0x32544652 && p.room && p.mode>=Duel && p.mode<=Raid
        && p.phase<=Feedback && p.signal<=Leaving && p.skill<=Bloom && p.energy<=100
        && p.hp0<=100 && p.hp1<=100 && p.boss<=145 && p.effect<=Heal
        && p.host<=1 && (p.mode!=Duel || validDuel(p.duel)) && p.turn<0x7fffffff;
}
struct Match {
    bool leader=false, peerAck=false;
    Arena arena;
    uint32_t room=0, client=0, revision=0, since=0;
    Skill skill=Flame;
    int energy=0;
    Signal signal=Idle;
    Phase phase=Lobby;
    Battle battle;
    void host(Mode mode,uint32_t id) {
        *this=Match(); leader=true; battle=Battle(mode); room=id;
    }
    void join(Mode mode,uint32_t id,uint32_t cookie) {
        *this=Match(); battle=Battle(mode); room=id; client=cookie;
    }
    bool myTurn() const { return battle.actor()==(leader ? 0 : 1); }
    void advance(Phase next,uint32_t now) {
        ++revision; phase=next; since=now; peerAck=false; signal=Idle;
    }
    void accept(Skill s,int power,uint32_t now) {
        skill=s; energy=clamp(power,0,100);
        battle.resolve(skill,energy);
        advance(Feedback,now);
    }
    bool cast(Skill s,int power,uint32_t now) {
        if (battle.mode==Duel && phase==Prepare) {
            if (!arena.submit(leader ? 0 : 1,s,power,arena.clockAt(leader,now))) return false;
            if (leader) arena.accept(0,arena.local,now);
            return true;
        }
        if (phase!=Prepare || !myTurn() || signal!=Idle || battle.finished()) return false;
        if (leader) accept(s,power,now);
        else { skill=s; energy=clamp(power,0,100); signal=Cast; }
        return true;
    }
    Packet message(uint32_t now=0) const {
        Packet p;
        p.room=room; p.client=client; p.revision=revision; p.host=leader;
        p.mode=battle.mode; p.phase=phase; p.turn=battle.turn;
        p.signal=signal; p.skill=skill; p.energy=energy;
        p.hp0=battle.hp[0]; p.hp1=battle.hp[1]; p.boss=battle.boss;
        p.effect=battle.effect;
        if (battle.mode==Duel) {
            p.duel.input=arena.local;
            p.duel.state=arena.state;
            if (leader) p.duel.state.clock=now;
        }
        return p;
    }
    bool receive(const Packet &p,uint32_t now) {
        if (!valid(p) || p.room!=room || p.mode!=battle.mode || bool(p.host)==leader
            || !p.client || (client && p.client!=client)) return false;
        if (leader) {
            if (phase==Lobby) {
                if (p.phase!=Lobby || p.revision!=0) return false;
                client=p.client; advance(Prepare,now);
            } else if (p.revision==revision && p.phase==phase && p.turn==uint32_t(battle.turn)) {
                if (battle.mode==Duel) {
                    arena.accept(1,p.duel.input,now);
                    peerAck=!arena.over() || (!p.duel.state.player[0].hp || !p.duel.state.player[1].hp);
                } else {
                    peerAck=true;
                    if (phase==Prepare && !myTurn() && p.signal==Cast) accept(Skill(p.skill),p.energy,now);
                }
            }
        } else if (p.revision>revision) {
            phase=Phase(p.phase); revision=p.revision; since=now;
            battle.turn=p.turn; battle.hp[0]=p.hp0; battle.hp[1]=p.hp1;
            battle.boss=p.boss; battle.effect=Effect(p.effect);
            skill=Skill(p.skill); energy=p.energy;
            signal=Idle;
        }
        if (!leader && battle.mode==Duel && p.revision==revision) arena.receive(p.duel.state,now);
        return true;
    }
    void tick(uint32_t now) {
        if (battle.mode==Duel && phase==Prepare) {
            if (leader) {
                bool wasOver=battle.finished();
                arena.accept(0,arena.local,now);
                arena.tick(now);
                if (!wasOver && arena.over()) peerAck=false;
            }
            for (int i=0;i<2;++i) battle.hp[i]=arena.state.player[i].hp;
            return;
        }
        if (leader && phase==Feedback && !battle.finished() && peerAck && now-since>=1750)
            advance(Prepare,now);
    }
};
}

