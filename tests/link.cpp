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
    assert(host.cast(Frost,60,now) && host.phase==Aim);
    exchange();
    guest.report(true,true);
    Packet result=guest.message();
    exchange();
    assert(host.battle.hp[0]==93 && host.battle.turn==1 && guest.battle.turn==1);
    uint32_t hostCue=0, guestCue=0;
    assert(!host.takeShieldCue(hostCue) && guest.takeShieldCue(guestCue));
    assert(!guest.takeShieldCue(guestCue));
    host.receive(result,now);
    assert(host.battle.turn==1);
    auto stale=host.message();
    for(int i=0;i<25;++i) exchange();
    assert(guest.phase==Prepare && guest.myTurn());
    guest.receive(stale,now);
    assert(guest.phase==Prepare);
    guest.cast(Flame,40,now);
    exchange();
    assert(host.phase==Aim && !host.myTurn());
    host.report(false,false);
    host.tick(now);
    exchange();
    assert(guest.battle.effect==Miss && guest.battle.hp[0]==93);
    assert(!guest.takeShieldCue(guestCue) && !host.takeShieldCue(hostCue));
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
    host.host(Duel,50); guest.join(Duel,50,22);
    exchange(); host.cast(Flame,100,now);
    now+=2400; host.tick(now);
    assert(host.battle.effect==Miss && host.battle.hp[1]==100);
    auto invalid=guest.message(); invalid.energy=101;
    assert(!host.receive(invalid,now));
    invalid=guest.message(); invalid.room=75;
    assert(!host.receive(invalid,now));
    puts("PASS room handshake, duel, raid victory, missing ACK, optical timeout and stale packets");
}

