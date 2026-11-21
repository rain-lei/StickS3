#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_system.h>
#include "display.h"
#include "radio.h"
#include "optical.h"
#include "peripherals.h"
#include "ir_check.h"

using namespace rift;
namespace {
const char *menus[]={"训练场","单人试炼","创建决斗","创建双人副本","寻找房间","现实机关"};
const char *skills[]={"火焰斩","冰霜箭","星鳞盾","回春阵"};
const char *moves[]={"横挥","前推","翻腕","画圈"};
const Tune sounds[]={Chirp,FireTone,IceTone,ShieldTone,HurtTone,ShieldTone,HealTone};
const char *slotKeys[]={"ir0","ir1","ir2"};
Preferences storage;
Peripherals device;
Pet pet;
Scene scene=Nest;
Skill selected=Frost;
Gesture motion;
Voice voice;
Battle trial;
Match match;
struct Room { Envelope advert; uint32_t seen=0; };
Room rooms[4];
unsigned roomIndex=0;
uint32_t slots[3]={}, now=0, lastInput=0, sceneAt=0, castAt=0, lastSample=0;
uint32_t lastCast=0, lastRender=0, lastPacket=0, lastPeer=0, lastShot=0, activeShot=0;
uint32_t lastFx=0, noteAt=0, noteLife=0, shownRevision=0, irStarted=0, fxSeen[2]={}, lampSeen=0;
uint32_t evolutionAt=0;
int lastLevel=1;
float ax=0,ay=0,az=1,gy=0;
int menu=0, runeAction=0, slot=0;
int shieldSlot=-1;
uint32_t shieldCode=0;
Effect fx=Waiting;
bool casting=false, sleepPet=false, rewarded=false, won=false, sideHeld=false;
bool receiving=false, shotSent=false, fxIncoming=false, hasFx=false, boardOK=false, storeOK=false;
const char *notice="";
uint8_t shotMask=0;
bool liveDuel() { return scene==Online && match.battle.mode==Duel; }
int playerIndex() { return match.leader ? 0 : 1; }
ui::Status noticeStyle=ui::Status::Notice;
void tell(const char *s, unsigned duration=1300, ui::Status style=ui::Status::Notice) {
    notice=s; noteAt=now; noteLife=duration; noticeStyle=style;
}
void savePet() {
    if (pet.level()>lastLevel) { lastLevel=pet.level(); evolutionAt=now; }
    if (!storeOK || storage.putBytes("dragon",&pet,sizeof(pet))!=sizeof(pet)) tell("存档写入失败",3000);
}
void stopCast() {
    if (casting) device.quiet();
    casting=false;
}
void stopRX() { device.stopListening(); receiving=false; }
void enter(Scene next) {
    stopCast(); stopRX(); scene=next; sceneAt=now; notice=""; noteLife=0;
}
bool networkScene() { return scene==Hosting || scene==Search || scene==Online; }
void leaveRoom() {
    if (networkScene() && match.client) {
        Packet goodbye=match.message(); goodbye.signal=Leaving;
        radio::send(goodbye);
    }
    radio::end();
}
void home() {
    leaveRoom(); enter(Nest);
    hasFx=false; rewarded=false; lastInput=now; lastCast=0;
}
void effect(Effect value) {
    fx=value; lastFx=now; hasFx=true;
    if (!casting) device.play(sounds[value]);
}
void finish(const Battle &b,int me) {
    if (rewarded || !b.finished()) return;
    rewarded=true; won=b.mode==Duel ? b.hp[me]>0 : b.boss==0;
    if (won) {
        pet.reward(b.mode!=Duel); savePet(); device.play(VictoryTone);
    } else device.play(HurtTone);
}
void startRoom(bool search,Mode kind) {
    if (!radio::begin()) { tell("无线启动失败"); return; }
    if (search) {
        for(auto &room : rooms) room.seen=0;
        roomIndex=0; match=Match(); enter(Search);
    } else {
        match.host(kind,esp_random()|1u); enter(Hosting);
    }
    rewarded=false; lastPacket=now-200; shownRevision=activeShot=lampSeen=0; fxSeen[0]=fxSeen[1]=0; lastPeer=now; hasFx=false;
}
void remember(const Envelope &in) {
    if (!in.data.host || in.data.phase!=Lobby || in.data.client) return;
    int available=-1;
    for(int i=0;i<4;++i) {
        if (rooms[i].seen && rooms[i].advert.data.room==in.data.room) { available=i; break; }
        if (available<0 && (!rooms[i].seen || now-rooms[i].seen>2500)) available=i;
    }
    if (available>=0) { rooms[available].advert=in; rooms[available].seen=now; }
}
bool fresh(int i) { return rooms[i].seen && now-rooms[i].seen<2500; }
void cycleRoom() {
    for(unsigned j=1;j<=4;++j) if (fresh((roomIndex+j)%4)) { roomIndex=(roomIndex+j)%4; break; }
}
void onTransition() {
    stopCast(); stopRX(); shotSent=false; lastShot=0;
    if (match.phase==Feedback) {
        effect(match.battle.effect);
        finish(match.battle, playerIndex());
    }
}
void serviceDuel() {
    auto &arena=match.arena;
    const auto &state=arena.state;
    const int me=playerIndex();
    const auto shot=state.active;
    // Local input stays independent of which optical transmission owns the IR hardware.
    arena.local.charging=casting ? selected+1 : 0;
    if (shot.proof()!=activeShot) {
        bool resumeVoice=receiving && casting;
        stopRX(); activeShot=shot.proof(); shotMask=0; shotSent=false;
        arena.local.ready=arena.local.observed=0;
        if (resumeVoice && (!shot.id || shot.receiver()!=me)) device.startVoice();
    }
    if (shot.id && !arena.over()) {
        uint32_t code=shotToken(match.room,shot.id,shot.reverse);
        if (shot.receiver()==me && !receiving && (!lastShot || now-lastShot>=285)) {
            receiving=device.listen(); irStarted=millis();
            if (!receiving) tell("红外接收器启动失败");
        }
        if (receiving) {
            if (uint32_t(millis()-irStarted)>=(shot.reverse ? 70u : shot.skill==Frost ? 700u : 450u)) arena.local.ready=shot.proof();
            while (device.readShot(code,shotMask)) {}
            if (completeShot(shotMask)) arena.local.observed=shot.proof();
        } else if (shot.receiver()!=me && state.ready==shot.proof() && (!shotSent || now-lastShot>=285)) {
            if (!shotSent && !casting) device.quiet();
            if (device.sendShot(code)) { shotSent=true; lastShot=now; }
        }
    }
    for (int i=0;i<2;++i) {
        const auto &p=state.player[i];
        if (p.event==fxSeen[i]) continue;
        fxSeen[i]=p.event; fxIncoming=i==me; effect(Effect(p.effect));
    }
    // A block belongs to the defender, even if other events arrive in the same update.
    if (!receiving && !shot.id && lampSeen!=state.player[me].blocks) {
        ++lampSeen;
        if (shieldSlot>=0 && !device.sendIR(shieldCode)) tell("格挡成功，灯光发送失败");
    }
    if (arena.over()) { stopCast(); stopRX(); finish(match.battle,me); }
}
void serviceRoom() {
    Envelope in;
    while(radio::next(in)) {
        if (scene==Search) { remember(in); continue; }
        if (scene==Hosting && match.leader && !match.client) {
            if (in.data.host || in.data.room!=match.room || in.data.mode!=match.battle.mode
                || !in.data.client || in.data.revision || in.data.phase!=Lobby || in.data.signal!=Idle) continue;
            if (!radio::bind(in.mac)) continue;
        } else if (!radio::fromPeer(in)) continue;
        if (!match.receive(in.data,now)) continue;
        lastPeer=now;
        if (in.data.signal==Leaving && !match.battle.finished()) {
            home(); tell("对方已离开",2400,ui::Status::Disconnected); return;
        }
    }
    if (scene==Search) { if (!fresh(roomIndex)) cycleRoom(); return; }
    if (scene==Hosting && match.phase!=Lobby) enter(Online);
    if ((scene==Online || !match.leader) && now-lastPeer>5500 && !match.battle.finished()) {
        home(); tell("连接超时",2500,ui::Status::Disconnected); return;
    }
    if (match.revision!=shownRevision) { shownRevision=match.revision; onTransition(); }
    match.tick(now);
    if (liveDuel()) serviceDuel();
    if (match.revision!=shownRevision) { shownRevision=match.revision; onTransition(); }
    if (now-lastPacket>=(liveDuel() ? 45u : 100u)) {
        radio::send(match.message(now),match.leader && match.phase==Lobby);
        lastPacket=now;
    }
}
void sampleMotion() {
    unsigned dt=now-lastSample;
    if (dt<10 || !M5.Imu.update()) return;
    lastSample=now;
    const auto &imu=M5.Imu.getImuData();
    ax=imu.accel.x; ay=imu.accel.y; az=imu.accel.z; gy=imu.gyro.y;
    if (casting) motion.sample(ax,ay,az,gy,dt);
    if (scene==Nest && ax*ax+ay*ay+az*az>2.8f && now-lastInput>650) {
        lastInput=now; sleepPet=false; device.play(Chirp); tell("小龙醒了",1300,ui::Status::Selection);
    }

}
int energy() {
    unsigned beat=(now-castAt)%600;
    int rhythm=(beat<85 || beat>515) && now-castAt>=500 ? 12 : 0;
    int combo=lastCast && now-lastCast<3300 ? 8 : 0;
    return clamp(voice.energy()+rhythm+combo+(motion.matches(Bloom) ? 15 : 0)
        +(pet.trained(selected) ? 10 : 0),0,100);
}
void beginCast(bool microphone) {
    if (casting) return;
    motion.start(ax,ay,az); voice=Voice(); castAt=now; casting=true;
    if (microphone) device.startVoice();
}
void release() {
    int power=energy();
    bool invoke=scene==Invoke || scene==Reward || scene==LinkRune;
    bool validMove=invoke ? motion.matches(Flame) || motion.matches(Frost) : motion.matches(selected);
    bool timely=now-castAt<=5500;
    stopCast();
    if (!validMove || !timely) { tell("动作未识别",1300,ui::Status::RetryGesture); return; }
    if (invoke) {
        if (scene==LinkRune) {
            shieldSlot=slot; shieldCode=slots[slot]; enter(Runes);
            tell("格挡联动已开启",1300,ui::Status::Blocked); return;
        }
        bool sent=device.sendIR(slots[slot]);
        if (sent) { enter(Runes); device.play(VictoryTone); tell("信号已发",1300,ui::Status::SignalSent); }
        else tell("红外发射失败");
        return;
    }
    lastCast=now;
    if (scene==Training) {
        pet.train(selected); effect(selected==Frost ? Freeze : selected==Flame ? Burn : selected==Ward ? Guard : Heal);
        tell(pet.practice[selected]==3 ? "法术已精通" : "训练成功",1300,ui::Status::Success); savePet();
    } else if (scene==Solo) {
        trial.resolve(selected,power); effect(trial.effect); finish(trial,0);
    } else if (scene==Online && !match.cast(selected,power,now)) {
        tell("技能未就绪");
    }
}
void startRune() {
    if (runeAction==3 && shieldSlot==slot) {
        shieldSlot=-1; shieldCode=0; tell("格挡联动已关闭"); return;
    }
    if (runeAction!=0 && !remoteCode(slots[slot])) { tell("先学习这个机关"); return; }
    if (runeAction==1 || runeAction==3) {
        if (!pet.runes) { tell("请先通关试炼"); return; }
        enter(runeAction==1 ? Invoke : LinkRune); return;
    }
    enter(runeAction==0 ? Learn : Challenge);
    if (!device.listen()) { enter(Runes); tell("红外接收启动失败"); }
}
void serviceRune() {
    uint32_t code;
    if (device.readIR(code)==IRRead::Decoded && remoteCode(code)) {
        if (scene==Learn) {
            bool saved=storeOK && storage.putUInt(slotKeys[slot],code)==sizeof(code);
            if (saved) {
                if (slot==shieldSlot && slots[slot]!=code) shieldSlot=-1;
                slots[slot]=code;
            }
            enter(Runes); tell(saved ? "已绑定" : "保存失败",1300,saved ? ui::Status::RemoteLearned : ui::Status::Notice); device.play(Chirp);
        } else if (code==slots[slot]) {
            enter(Runes); pet.xp+=3; tell("机关挑战成功",1300,ui::Status::Success); savePet(); device.play(VictoryTone);
        } else tell("遥控键不匹配");
    } else if (now-sceneAt>=9000) { enter(Runes); tell("接收超时",1300,ui::Status::LearnTimeout); }
}
void input(bool aDown,bool aUp,bool bTap) {
    if (scene==Nest) {
        if (aDown || bTap) {
            lastInput=now;
            if (sleepPet) { sleepPet=false; device.play(Chirp); return; }
        }
        if (bTap) menu=(menu+1)%6;
        if (!aDown) return;
        rewarded=false;
        switch(menu) {
            case 0: enter(Training); break;
            case 1: trial=Battle(Trial); enter(Solo); device.play(BattleTone); break;
            case 2: startRoom(false,Duel); break;
            case 3: startRoom(false,Raid); break;
            case 4: startRoom(true,Duel); break;
            case 5: enter(Runes); break;
        }
        return;
    }
    if (scene==Search) {
        if (bTap) cycleRoom();
        if (aDown && fresh(roomIndex)) {
            const auto &room=rooms[roomIndex].advert;
            if (!radio::bind(room.mac)) { tell("连接失败"); return; }
            match.join(Mode(room.data.mode),room.data.room,esp_random()|1u);
            enter(Hosting); lastPeer=now; lastPacket=now-100;
        }
        return;
    }
    if (scene==Runes) {
        if (bTap) { if (++slot==3) { slot=0; runeAction=(runeAction+1)%4; } }
        if (aDown) startRune();
        return;
    }
    if (scene==Learn || scene==Challenge || scene==Hosting) return;
    if ((scene==Solo || scene==Online) && rewarded) {
        if (aDown && (scene==Solo || !match.leader || match.peerAck || now-lastPeer>5500)) {
            bool reward=won && (scene==Solo || match.battle.mode==Raid);
            home();
            if (reward) { slot=0; enter(Reward); }
        }
        return;
    }
    if (scene==Online && !liveDuel() && (!match.myTurn() || match.phase!=Prepare || match.signal!=Idle)) return;
    if (scene==Solo && hasFx && now-lastFx<1400) return;
    bool physical=scene==Invoke || scene==Reward || scene==LinkRune;
    if (physical) {
        if (bTap && !casting && scene!=LinkRune) slot=(slot+1)%3;
        if (aDown && !remoteCode(slots[slot])) { tell("请先绑定此槽"); return; }
    } else if (bTap && !casting) selected=Skill((selected+1)%4);
    if (aDown && liveDuel()) {
        int me=playerIndex();
        auto &arena=match.arena;
        auto &p=arena.state.player[me];
        if (arena.remaining(me,selected,arena.clockAt(match.leader,now))) { tell("技能冷却中",650); return; }
        if (selected==Ward && p.layers==maxLayers) { tell("护盾已叠满",650); return; }
        if (selected==Bloom && p.hp==100) { tell("生命已满",650); return; }
    }
    if (aDown) beginCast(!physical && !receiving);
    if (casting && aUp) release();
}
void publishWatch() {
    static uint32_t sentAt=0;
    if (!watchEnabled(now) || now-sentAt<125) return;
    sentAt=now;
    const auto &arena=match.arena;
    char players[2][84];
    for (int i=0;i<2;++i) {
        const auto &p=arena.state.player[i];
        unsigned cd[4]={};
        if (liveDuel()) for (int k=0;k<4;++k) cd[k]=arena.remaining(i,Skill(k),arena.clockAt(match.leader,now));
        snprintf(players[i],sizeof(players[i]),"[%u,%u,%u,%u,%lu,%u,%u,%u,%u]",p.hp,p.layers,p.charging,
                 p.effect,(unsigned long)p.event,cd[0],cd[1],cd[2],cd[3]);
    }
    const auto &shot=arena.state.active;
    char line[240];
    int n=snprintf(line,sizeof(line),"RIFT LIVE [1,%d,%d,%lu,%d,%s,%s,[%lu,%u,%u,%u]]\n",
                   int(scene),networkScene() ? int(match.battle.mode) : 0,
                   (unsigned long)(networkScene() ? match.room : 0),playerIndex(),players[0],players[1],
                   (unsigned long)shot.id,shot.actor,shot.skill,shot.reverse);
    if (n>0 && n<int(sizeof(line)) && Serial.availableForWrite()>=n) Serial.write((const uint8_t*)line,n);
}
ui::Status effectStatus(Effect value, bool incoming) {
    switch (value) {
        case Burn:   return incoming ? ui::Status::TookHit : ui::Status::Hit;
        case Freeze: return incoming ? ui::Status::Frozen : ui::Status::Hit;
        case Block:  return incoming ? ui::Status::Blocked : ui::Status::OtherBlocked;
        case Miss:   return ui::Status::Miss;
        case Guard:  return ui::Status::GuardRaised;
        case Heal:   return ui::Status::Healed;
        default:     return ui::Status::Judging;
    }
}

void battleView(View& frame) {
    ui::BoardView& v=frame.status;
    const bool online = scene == Online;
    const Battle& battle = online ? match.battle : trial;
    v.mode = scene == Training ? ui::Mode::Training :
             !online ? ui::Mode::Trial :
             battle.mode == Duel ? ui::Mode::Duel : ui::Mode::CoOp;

    if (scene != Training) {
        const int me=online && !match.leader ? 1 : 0;
        frame.hp=battle.hp[me];
        frame.enemy=battle.mode==Duel ? battle.hp[1-me] : battle.boss;
        frame.enemyMax=battle.mode==Raid ? 145 : 100;
        if (battle.mode==Raid) frame.ally=battle.hp[1-me];
        const bool feedback = online ? match.phase == Feedback : hasFx && now - lastFx < 1400;
        if (rewarded || feedback) {
            if (battle.mode == Raid)
                snprintf(v.detail, sizeof(v.detail), "我%d 友%d 敌%d", frame.hp, frame.ally, frame.enemy);
            else
                snprintf(v.detail, sizeof(v.detail), "我%d 敌%d", frame.hp, frame.enemy);
        }
        if (rewarded) {
            v.status = won ? ui::Status::Victory : ui::Status::Defeat;
            const bool canContinue = !online || !match.leader || match.peerAck || now-lastPeer>5500;
            v.setPrompt(won ? "胜利" : "挑战失败", canContinue ? "A 继续" : "等待同步");
            return;
        }
        if (liveDuel()) {
            const auto &arena=match.arena;
            const auto &opponent=arena.state.player[1-me];
            frame.layers=arena.state.player[me].layers;
            frame.enemyLayers=opponent.layers;
            frame.enemyCharging=opponent.charging;
            const auto &shot=arena.state.active;
            if (shot.id) { frame.flight=shot.skill; frame.hostileShot=shot.actor!=me; }
            const uint32_t clock=arena.clockAt(match.leader,now);
            for (int i=0;i<4;++i) frame.cooldown[i]=arena.remaining(me,Skill(i),clock);
            if (casting) {
                v.status=ui::Status::Charging;
                v.chargePercent=static_cast<uint8_t>(frame.energy);
                v.onBeat=frame.onBeat;
                char cue[64];
                snprintf(cue,sizeof(cue),"%s %d%%  %s",skills[selected],frame.energy,motion.matches(selected) ? "可释放" : moves[selected]);
                v.setPrompt(cue);
            } else if (frame.hostileShot) {
                v.status=ui::Status::Judging;
                v.setPrompt(frame.flight==Frost ? "冰箭来袭 · 可叠盾" : "火斩来袭 · 可叠盾");
            } else if (hasFx && frame.fxAge<750) {
                v.status=effectStatus(fx,fxIncoming);
                if (fx==Burn || fx==Freeze) {
                    const bool fire=fx==Burn;
                    if (!fxIncoming) v.setPrompt(fire ? "火斩命中" : "冰箭命中");
                } else if (fx==Guard) v.setPrompt(fxIncoming ? "护盾展开" : "对方展开护盾");
                else if (fx==Heal) v.setPrompt(fxIncoming ? "恢复生命" : "对方恢复生命");
                else if (fx==Miss) v.setPrompt(fxIncoming ? "未命中" : "对方未命中");
            } else {
                v.status=ui::Status::Ready;
                char cue[64];
                snprintf(cue,sizeof(cue),"%s  按 A %s",skills[selected],moves[selected]);
                v.setPrompt(cue);
            }
            return;
        }
        if (feedback) {
            v.status = effectStatus(battle.effect, false);
            if (battle.effect==Burn || battle.effect==Freeze) {
                const bool fire=battle.effect==Burn;
                v.skill=fire ? ui::Skill::FireSlash : ui::Skill::IceArrow;
                v.setPrompt(fire ? "火斩命中" : "冰箭命中");
            }
            return;
        }
        if (online && (!match.myTurn() || match.signal != Idle)) {
            v.status = ui::Status::WaitingForPlayer;
            if (match.myTurn()) v.setPrompt("已出手", "等待判定");
            else v.setPrompt("队友回合");
            return;
        }
    }
    if (!casting) {
        v.status = ui::Status::Ready;
        if (scene != Training && battle.mode != Duel) {
            v.setPrompt("按住 A", battle.omen()==Frost ? "敌相冰 / B 换技" : "敌相火 / B 换技");
        }
    } else {
        v.status = ui::Status::Charging;
        v.chargePercent = static_cast<uint8_t>(frame.energy);
        v.onBeat = frame.onBeat;
        if (motion.matches(selected)) v.setPrompt("松开 A", "释放当前法术");
        else v.setPrompt(moves[selected], "按住 A 蓄力");
    }
}

void render() {
    View frame;
    ui::BoardView& v=frame.status;
    char header[80]={};
    frame.title=header;
    frame.scene=scene;
    frame.level=pet.level();
    frame.levelXp=frame.level==30 ? 30 : pet.xp%30;
    frame.evolved=frame.level>=2;
    frame.evolving=evolutionAt && now-evolutionAt<1700;
    frame.asleep=scene==Nest && sleepPet;
    frame.skillName=skills[selected];
    frame.mastery=pet.practice[selected];
    frame.roomMode=match.battle.mode;
    frame.rivalDragon=liveDuel();
    frame.skill=selected;
    frame.incoming=frame.rivalDragon && fxIncoming;
    frame.charging=casting;
    frame.energy=casting ? energy() : 0;
    const unsigned beat=(now-castAt)%600;
    frame.onBeat=beat<85 || beat>515;
    frame.effect=fx; frame.fxAge=hasFx ? now-lastFx : 9999;
    static constexpr ui::Skill spellIcons[] = {ui::Skill::FireSlash, ui::Skill::IceArrow,
                                               ui::Skill::Shield, ui::Skill::Heal};
    v.skill = spellIcons[selected];
    switch (scene) {
        case Nest:
            snprintf(header,sizeof(header),"%s / XP %lu",pet.wins ? "守隙者" : "初生",(unsigned long)pet.xp);
            v.status = ui::Status::Selection;
            if (sleepPet) v.setPrompt("休眠中", "按键或轻摇");
            else v.setPrompt(menus[menu], "A 进入 / B 切换");
            break;
        case Runes: case Learn: case Challenge: case Invoke: case Reward: case LinkRune: {
            v.mode = ui::Mode::Mechanism;
            v.slot = static_cast<uint8_t>(slot + 1);
            snprintf(v.detail, sizeof(v.detail), "%d / %s", slot + 1,
                     remoteCode(slots[slot]) ? "已绑定" : "空槽");
            snprintf(header,sizeof(header),"符文 %lu  ·  %s",(unsigned long)pet.runes,v.detail);
            if (scene == Runes) {
                const char* actions[] = {"学习遥控", "施放机关", "符文挑战",
                                         slot == shieldSlot ? "解除联动" : "格挡联动"};
                v.status = ui::Status::Selection;
                v.setPrompt(actions[runeAction], "A 选择 / B 切换");
            } else if (scene == Learn || scene == Challenge) {
                v.status = ui::Status::LearnRemote;
                if (scene == Challenge) v.setPrompt("按遥控键", "匹配选中机关");
            } else {
                v.status = ui::Status::AimAtDevice;
                if (!remoteCode(slots[slot])) v.setPrompt("空机关槽", "长按 B 回巢绑定");
                else if (casting) v.setPrompt("挥杖松 A", scene == LinkRune ? "确认格挡联动" : "对准所选设备");
                else if (scene == LinkRune) v.setPrompt("绑定格挡", "按住 A 挥杖");
                else v.setPrompt("按住 A 挥杖", "B 换机关");
            }
            break;
        }
        case Search:
            frame.title="寻找房间";
            v.mode = ui::Mode::Lobby;
            v.status = ui::Status::WaitingForPlayer;
            if (fresh(roomIndex)) {
                const auto& room = rooms[roomIndex].advert.data;
                snprintf(v.detail, sizeof(v.detail), "#%04X", unsigned(room.room & 65535));
                frame.title=v.detail;
                v.setPrompt(room.mode == Duel ? "决斗房间" : "共斗房间", "A 加入 / B 换房");
            } else v.setPrompt("搜寻房间", "长按 B 返回");
            break;
        case Hosting:
            v.mode = ui::Mode::Lobby;
            v.status = ui::Status::WaitingForPlayer;
            snprintf(v.detail, sizeof(v.detail), "#%04X", unsigned(match.room & 65535));
            snprintf(header,sizeof(header),"房间 %s",v.detail);
            v.setPrompt(match.leader ? "等待加入" : "正在加入", "长按 B 返回");
            break;
        case Training: case Solo: case Online: {
            battleView(frame);
            if (scene==Training) frame.title="法术训练";
            else if (v.detail[0]) frame.title=v.detail;
            else snprintf(header,sizeof(header),"%s / %s",v.mode==ui::Mode::Duel ? "实时决斗" :
                          v.mode==ui::Mode::CoOp ? "双人裂隙龙" : "裂隙龙",skills[selected]);
            break;
        }
    }
    if (now - noteAt < noteLife && notice[0]) {
        frame.hasNotice=true;
        v.status = noticeStyle;
        if (noticeStyle == ui::Status::Disconnected) v.mode = ui::Mode::Lobby;
        v.setPrompt(notice);
    } else if (scene == Nest && evolutionAt && now - evolutionAt < 1700) {
        v.status = ui::Status::Success;
        v.setPrompt("小龙进化");
    }
    draw(frame,now);
}
}
void setup() {
    Serial.begin(115200);
    M5.begin(M5.config());
    boardOK=M5.getBoard()==m5::board_t::board_M5StickS3;
    if (!boardOK) { M5.Display.print("RIFT requires M5Stack StickS3"); return; }
    initDisplay();
    storeOK=storage.begin("rift-v1",false);
    if (storeOK && storage.getBytesLength("dragon")==sizeof(Pet)) {
        Pet stored;
        if (storage.getBytes("dragon",&stored,sizeof(stored))==sizeof(stored) && stored.version==1) pet=stored;
    }
    for (int i=0;i<3;++i) slots[i]=storeOK ? storage.getUInt(slotKeys[i],0) : 0;
    lastLevel=pet.level();
    now=lastInput=millis();
    if (!device.initIR()) tell("红外初始化失败",4000);
    if (!storeOK) tell("存档不可用",4000);
    device.play(Chirp);
}
void loop() {
    if (!boardOK) { delay(100); return; }
    M5.update(); now=millis();
    if (pollIRCheck(device,scene==Nest)) { delay(2); return; }
    sampleMotion();
    if (networkScene()) serviceRoom();
    if (scene==Learn || scene==Challenge) serviceRune();
    if (scene==Nest && now-lastInput>20000) sleepPet=true;
    if (M5.BtnB.isPressed() && M5.BtnB.pressedFor(750) && !sideHeld) {
        sideHeld=true; home();
    }
    bool tap=M5.BtnB.wasReleased() && !sideHeld;
    if (M5.BtnB.wasReleased()) sideHeld=false;
    input(M5.BtnA.wasPressed(),M5.BtnA.wasReleased(),tap);
    if (casting) {
        device.updateVoice(voice);
        if (now-castAt>5500) { stopCast(); tell("蓄力超时",1300,ui::Status::RetryGesture); }
    }
    publishWatch();
    device.updateAudio(millis());
    if (now-lastRender>=50) { render(); lastRender=now; }
    delay(2);
}

