#pragma once
#include "rules.h"

namespace rift {
constexpr uint32_t cooldownMs[]={2400,3600,4200,7500};
constexpr uint8_t maxLayers=3;
constexpr uint32_t forwardWindowMs=1120, reverseWindowMs=1650;

struct DuelInput {
    uint32_t cast[4]={};
    uint8_t power[4]={};
    uint32_t ready=0, observed=0;
    uint8_t charging=0; // 1 + skill while the button is held
};
struct Fighter {
    uint32_t seen[4]={}, used[4]={};
    uint32_t event=0, blocks=0, chilledAt=0;
    uint8_t chilled=0, usedMask=0;
    uint8_t hp=100, layers=0, effect=Waiting, charging=0;
};
struct Shot {
    uint32_t id=0;
    uint8_t actor=0, skill=Flame, power=0, reverse=0;
    uint32_t proof() const { return id ? 2*id+reverse : 0; }
    int receiver() const { return reverse ? actor : 1-actor; }
};
struct DuelSnapshot {
    Fighter player[2];
    Shot active;
    uint32_t ready=0, clock=0;
};
struct DuelWire {
    DuelSnapshot state;
    DuelInput input;
};
inline bool validDuel(const DuelWire &wire) {
    const auto &s=wire.state;
    if (s.active.reverse>1 || s.active.actor>1 || s.active.skill>Frost || s.active.power>100 || wire.input.charging>4) return false;
    for (auto &p:s.player) if (p.hp>100 || p.layers>maxLayers || p.effect>Heal || p.charging>4) return false;
    for (auto power:wire.input.power) if (power>100) return false;
    return true;
}
struct Arena {
    DuelSnapshot state;
    DuelInput local;
    Shot queue[4]; // At most one outstanding shot per offensive skill and player.
    uint32_t sequence=0, started=0, receivedAt=0;
    bool synced=false;
    bool over() const { return !state.player[0].hp || !state.player[1].hp; }
    uint32_t remaining(int actor,Skill skill,uint32_t clock) const {
        const auto &p=state.player[actor];
        uint32_t elapsed=clock-p.used[skill];
        uint32_t cd=(p.usedMask & (1u<<skill)) && elapsed<cooldownMs[skill] ? cooldownMs[skill]-elapsed : 0;
        uint32_t chillAge=clock-p.chilledAt;
        if (skill<=Frost && p.chilled && chillAge<800) cd=std::max(cd,800u-chillAge);
        return cd;
    }
    bool submit(int actor,Skill skill,int power,uint32_t clock) {
        const auto &p=state.player[actor];
        if (over() || local.cast[skill]!=p.seen[skill] || remaining(actor,skill,clock)
            || (skill==Ward && p.layers==maxLayers) || (skill==Bloom && p.hp==100)) return false;
        ++local.cast[skill]; local.power[skill]=clamp(power,0,100);
        return true;
    }
    static void event(Fighter &p,Effect fx) { ++p.event; p.effect=fx; }
    void accept(int actor,const DuelInput &in,uint32_t now) {
        auto &p=state.player[actor];
        if (over()) return;
        p.charging=in.charging;
        for (int i=0;i<4;++i) {
            Skill skill=Skill(i);
            if (in.cast[i]!=p.seen[i]+1) continue;
            bool available=!remaining(actor,skill,now);
            p.seen[i]=in.cast[i]; // Also acknowledge declined or stale submissions.
            if (!available) continue;
            if (skill==Ward) {
                if (p.layers==maxLayers) continue;
                ++p.layers; event(p,Guard);
            } else if (skill==Bloom) {
                if (p.hp==100) continue;
                p.hp=clamp(p.hp+18+in.power[i]/8,0,100); event(p,Heal);
            } else {
                Shot &slot=queue[actor*2+i];
                if (slot.id) continue;
                slot.id=++sequence; slot.actor=actor; slot.skill=i; slot.power=in.power[i];
            }
            p.used[i]=now; p.usedMask|=1u<<i;
        }
        auto shot=state.active;
        if (!shot.id || actor!=shot.receiver()) return;
        if (in.ready==shot.proof() && state.ready!=shot.proof()) { state.ready=shot.proof(); started=now; }
        if (state.ready==shot.proof() && in.observed==shot.proof()) resolve(true,now);
    }
    void resolve(bool hit,uint32_t now) {
        const Shot shot=state.active;
        auto &caster=state.player[shot.actor];
        auto &target=state.player[1-shot.actor];
        if (!hit) event(caster,Miss);
        else if (target.layers) {
            --target.layers; ++target.blocks; event(target,Block);
            caster.hp=clamp(caster.hp-7,0,100);
        } else {
            int damage=(shot.skill==Flame ? 18 : 24)+shot.power/8;
            target.hp=clamp(target.hp-damage,0,100);
            if (shot.skill==Frost) { target.chilled=1; target.chilledAt=now; }
            event(target,shot.skill==Flame ? Burn : Freeze);
        }
        state.active=Shot(); state.ready=0;
    }
    void tick(uint32_t now) {
        for (auto &p:state.player) if (now-p.chilledAt>=800) p.chilled=0;
        if (over()) { state.active=Shot(); state.ready=0; return; }
        uint32_t window=state.active.reverse ? reverseWindowMs : forwardWindowMs;
        if (state.active.id && now-started >= (state.ready ? window : 2300u)) {
            if (state.active.reverse) resolve(false,now);
            else { state.active.reverse=1; state.ready=0; started=now; }
        }
        if (state.active.id) return;
        Shot *first=nullptr;
        for (auto &s:queue) if (s.id && (!first || s.id<first->id)) first=&s;
        if (first) { state.active=*first; *first=Shot(); started=now; }
    }
    void receive(const DuelSnapshot &s,uint32_t now) {
        if (synced && int32_t(s.clock-state.clock)<=0) return;
        state=s; receivedAt=now; synced=true;
    }
    uint32_t clockAt(bool host,uint32_t now) const { return host ? now : state.clock+(now-receivedAt); }
};
}
