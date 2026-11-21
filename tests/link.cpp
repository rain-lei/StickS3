#include "../src/link.h"
#include <cassert>
#include <cstdio>
using namespace rift;
int main() {
    Match host, guest;
    host.host(Duel, 1742); guest.join(Duel,1742,82);
    uint32_t now=100;
    auto exchange=[&] {
        host.receive(guest.message(),now);
        guest.receive(host.message(),now);
        now+=100;
        host.tick(now);
    };
    exchange();
    assert(host.phase==Prepare && guest.phase==Prepare);
    // Both attacks are legal at the same instant; neither side waits to defend.
    assert(host.cast(Flame,60,now));
    assert(guest.cast(Frost,60,now));
    assert(guest.cast(Ward,0,now));
    assert(!host.cast(Flame,60,now));
    auto inputs=guest.message(now);
    host.receive(inputs,now); host.receive(inputs,now);
    host.tick(now);
    auto &arena=host.arena;
    assert(arena.state.active.actor==0 && arena.state.player[1].layers==1);
    auto shot=arena.state.active.proof();
    inputs.duel.input.ready=inputs.duel.input.observed=shot+99;
    host.receive(inputs,now);
    assert(arena.state.player[1].layers==1);
    inputs.duel.input.ready=inputs.duel.input.observed=shot;
    host.receive(inputs,now); host.receive(inputs,now);
    assert(arena.state.player[1].layers==0 && arena.state.player[1].blocks==1);
    assert(arena.state.player[0].hp==93 && arena.state.player[1].hp==100);
    host.tick(now);
    assert(arena.state.active.actor==1 && arena.state.active.skill==Frost);
    auto stale=host.message(now);
    arena.local.ready=arena.state.active.proof();
    host.tick(++now);
    now+=forwardWindowMs; host.tick(now);
    assert(arena.state.active.reverse && arena.state.active.receiver()==1);
    // A late forward report, or our own earlier light, cannot complete the reverse probe.
    arena.local.observed=arena.local.ready;
    host.tick(now);
    assert(arena.state.active.id && !arena.state.ready);
    inputs.duel.input.ready=arena.state.active.proof();
    host.receive(inputs,now);
    now+=reverseWindowMs; host.tick(now);
    assert(!arena.state.active.id && arena.state.player[0].hp==93);
    guest.receive(host.message(now),now);
    guest.receive(stale,now);
    assert(!guest.arena.state.active.id && guest.arena.state.player[0].hp==93);
    assert(host.cast(Bloom,0,now) && arena.state.player[0].hp==100);
    assert(!host.cast(Bloom,0,now));
    for (int n=0;n<3;++n) {
        now+=cooldownMs[Ward];
        assert(host.cast(Ward,0,now));
        assert(!host.cast(Ward,0,now));
    }
    now+=cooldownMs[Ward];
    assert(!host.cast(Ward,0,now) && arena.state.player[0].layers==3);
    // Lost submissions repeat on every snapshot; a confirmed lethal hit settles once.
    arena.state.player[1].hp=1;
    assert(host.cast(Flame,0,now)); host.tick(now);
    inputs.duel.input.ready=inputs.duel.input.observed=arena.state.active.proof();
    host.receive(inputs,now); host.tick(now);
    assert(host.battle.finished() && !host.peerAck && !host.cast(Ward,0,now));
    guest.receive(host.message(now),now); guest.tick(now);
    assert(guest.battle.finished());
    host.receive(guest.message(now),now);
    assert(host.peerAck);
    Arena frost;
    const uint32_t beforeWrap=0xffffff00u;
    assert(frost.submit(0,Frost,50,beforeWrap));
    frost.accept(0,frost.local,beforeWrap); frost.tick(beforeWrap);
    DuelInput defense;
    frost.tick(beforeWrap+2300u); // No forward READY. Caster receives the distinct return proof.
    assert(frost.state.active.reverse);
    defense.ready=defense.observed=frost.state.active.proof();
    frost.accept(0,defense,beforeWrap+2300u);
    assert(frost.state.player[1].hp==70);
    assert(!frost.submit(1,Flame,30,beforeWrap+2700u));
    assert(frost.submit(1,Ward,30,beforeWrap+2700u));
    assert(frost.remaining(1,Flame,beforeWrap+3099u)==1);
    assert(frost.remaining(1,Flame,beforeWrap+3100u)==0);
    Match stranger;
    stranger.join(Duel,1742,999);
    assert(!host.receive(stranger.message(),now));
    // Drop initial submission and first server feedback during an entire raid.
    host.host(Raid,7236); guest.join(Raid,7236,331);
    exchange();
    while (!host.battle.finished()) {
        Match &caster=host.myTurn() ? host : guest;
        assert(caster.cast(Frost,100,now));
        if (!host.myTurn() && host.phase==Prepare) host.receive(guest.message(),now);
        assert(host.phase==Feedback);
        for(int i=0;i<22;++i) { now+=100; host.tick(now); }
        assert(host.phase==Feedback); // no application-level acknowledgment yet
        exchange();
        assert(guest.battle.boss==host.battle.boss && guest.battle.turn==host.battle.turn);
        exchange();
        exchange();
    }
    assert(guest.battle.finished() && guest.battle.boss==0);
    auto invalid=guest.message(); invalid.energy=101;
    assert(!host.receive(invalid,now));
    invalid=guest.message(); invalid.room=75;
    assert(!host.receive(invalid,now));
    puts("PASS simultaneous casts, stacked shields, cooldowns, required optical proof, retries, final ACK and raid");
}

