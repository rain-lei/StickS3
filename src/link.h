#pragma once
#include "rules.h"

namespace rift {
enum Phase : uint8_t { Lobby, Prepare, Aim, Feedback };
enum Signal : uint8_t { Idle, Cast, Listening, Observed, Leaving };

// Both devices run the same little-endian firmware. Fixed width wire fields only.
struct Packet {
    uint32_t magic=0x31544652, room=0, client=0, revision=0;
    uint32_t turn=0;
    uint8_t host=0, mode=0, phase=0, signal=0, skill=0, energy=0;
    uint8_t hp0=100,hp1=100,boss=0,effect=0, hit=0, block=0;
};
static_assert(sizeof(Packet)==32,"Unexpected room packet layout");
inline bool valid(const Packet &p) {
    return p.magic==0x31544652 && p.room && p.mode>=Duel && p.mode<=Raid
        && p.phase<=Feedback && p.signal<=Leaving && p.skill<=Bloom && p.energy<=100
        && p.hp0<=100 && p.hp1<=100 && p.boss<=145 && p.effect<=Heal
        && p.host<=1 && p.hit<=1 && p.block<=1 && p.turn<0x7fffffff;
}
struct Match {
    bool leader=false, peerAck=false, ready=false, hit=false, blocked=false;
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
    bool takeShieldCue(uint32_t &lastPlayed) const {
        // In feedback the turn has advanced, so the defender is now the actor.
        if (phase!=Feedback || battle.mode!=Duel || battle.effect!=Block || !myTurn()
            || (leader && !peerAck) || lastPlayed==revision) return false;
        lastPlayed=revision;
        return true;
    }
    void advance(Phase next,uint32_t now) {
        ++revision; phase=next; since=now; peerAck=ready=hit=blocked=false; signal=Idle;
    }
    void settle(bool success,bool guard,uint32_t now) {
        battle.resolve(skill,energy,success,guard);
        advance(Feedback,now);
    }
    void accept(Skill s,int power,uint32_t now) {
        skill=s; energy=clamp(power,0,100);
        if (battle.mode==Duel && (s==Frost || s==Flame)) advance(Aim,now);
        else settle(true,false,now);
    }
    bool cast(Skill s,int power,uint32_t now) {
        if (phase!=Prepare || !myTurn() || signal!=Idle || battle.finished()) return false;
        if (leader) accept(s,power,now);
        else { skill=s; energy=clamp(power,0,100); signal=Cast; }
        return true;
    }
    void listening() {
        if (phase==Aim && !myTurn() && signal==Idle) { signal=Listening; ready=true; }
    }
    void report(bool success,bool guard) {
        if (phase!=Aim || myTurn() || signal==Observed) return;
        signal=Observed; hit=success; blocked=success && guard;
    }
    Packet message() const {
        Packet p;
        p.room=room; p.client=client; p.revision=revision; p.host=leader;
        p.mode=battle.mode; p.phase=phase; p.turn=battle.turn;
        p.signal=signal; p.skill=skill; p.energy=energy;
        p.hp0=battle.hp[0]; p.hp1=battle.hp[1]; p.boss=battle.boss;
        p.effect=battle.effect; p.hit=hit; p.block=blocked;
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
                peerAck=true;
                if (phase==Prepare && !myTurn() && p.signal==Cast) accept(Skill(p.skill),p.energy,now);
                else if (phase==Aim && myTurn()) {
                    if (p.signal==Listening) ready=true;
                    if (p.signal==Observed) settle(p.hit,p.block,now);
                }
            }
        } else if (p.revision>revision) {
            phase=Phase(p.phase); revision=p.revision; since=now;
            battle.turn=p.turn; battle.hp[0]=p.hp0; battle.hp[1]=p.hp1;
            battle.boss=p.boss; battle.effect=Effect(p.effect);
            skill=Skill(p.skill); energy=p.energy;
            signal=Idle; ready=hit=blocked=false;
        }
        if (!leader && phase==Aim && p.revision==revision && p.signal==Listening) ready=true;
        return true;
    }
    void tick(uint32_t now) {
        if (!leader) return;
        if (phase==Aim && !myTurn() && signal==Observed) settle(hit,blocked,now);
        else if (phase==Aim && now-since>=2300) settle(false,false,now);
        else if (phase==Feedback && !battle.finished() && peerAck && now-since>=1750)
            advance(Prepare,now);
    }
};
}

