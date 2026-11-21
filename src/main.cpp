#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_system.h>
#include "display.h"
#include "radio.h"
#include "optical.h"
#include "peripherals.h"

using namespace rift;
namespace {
const char *menus[]={"训练场","单人试炼","创建决斗","创建双人副本","寻找房间","现实机关"};
const char *skills[]={"火焰斩","冰霜箭","星鳞盾","回春阵"};
const char *moves[]={"横挥","前推","翻腕","画圈"};
const char *effects[]={"准备施法","火焰命中","冰冻结晶","格挡反击","未命中","护盾展开","回复生命"};
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
uint32_t lastCast=0, lastRender=0, lastPacket=0, lastPeer=0, lastShot=0, guardAt=0;
uint32_t lastFx=0, noteAt=0, noteLife=0, shownRevision=0, irStarted=0;
uint32_t evolutionAt=0;
int lastLevel=1;
float ax=0,ay=0,az=1,gy=0,guardRoll=0;
int menu=0, runeAction=0, slot=0;
int shieldSlot=-1;
uint32_t shieldCode=0, shieldRevision=0;
Effect fx=Waiting;
bool casting=false, sleepPet=false, rewarded=false, won=false, sideHeld=false;
bool receiving=false, shotSent=false, guardSet=false, hasFx=false, boardOK=false, storeOK=false;
const char *notice="";
void tell(const char *s,unsigned duration=1300) { notice=s; noteAt=now; noteLife=duration; }
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
    device.play(sounds[value]);
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
    rewarded=false; lastPacket=now-200; shownRevision=shieldRevision=0; lastPeer=now; guardSet=false;
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
        finish(match.battle, match.leader ? 0 : 1);
    } else if (match.phase==Prepare) { guardRoll=0; guardSet=false; }
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
            home(); tell("队友离开了房间",2400); return;
        }
    }
    if (scene==Search) { if (!fresh(roomIndex)) cycleRoom(); return; }
    if (scene==Hosting && match.phase!=Lobby) enter(Online);
    if ((scene==Online || !match.leader) && now-lastPeer>5500 && !match.battle.finished()) {
        home(); tell("连接超时，可重新建房",2500); return;
    }
    if (match.revision!=shownRevision) { shownRevision=match.revision; onTransition(); }
    if (shieldSlot>=0 && match.takeShieldCue(shieldRevision)) {
        if (!device.sendIR(shieldCode)) tell("格挡成功，灯光发送失败");
    }
    match.tick(now);
    if (match.revision!=shownRevision) { shownRevision=match.revision; onTransition(); }
    if (match.phase==Aim) {
        uint32_t code=token(match.room,match.battle.turn);
        if (!match.myTurn() && match.signal!=Observed) {
            if (!receiving) {
                receiving=device.listen(); irStarted=now;
                if (!receiving) { match.report(false,false); tell("红外启动失败"); }
            }
            if (receiving && now-irStarted>=35) match.listening();
            uint32_t got;
            if (receiving && device.readIR(got) && got==code)
                match.report(true,guardSet && now-guardAt<1050);
            else if (receiving && now-irStarted>=1300) match.report(false,false);
            if (match.signal==Observed) stopRX();
        } else if (match.myTurn() && match.ready && (!shotSent || now-lastShot>=240)) {
            device.quiet();
            if (!device.sendIR(code)) tell("红外发射失败");
            shotSent=true; lastShot=now;
        }
    }
    if (now-lastPacket>=100) {
        radio::send(match.message(),match.leader && match.phase==Lobby);
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
        lastInput=now; sleepPet=false; device.play(Chirp); tell("小龙回应了你的摇晃");
    }
    if (scene==Online && !match.myTurn() && (match.phase==Prepare || match.phase==Aim)) {
        if (fabsf(gy)<35) guardRoll=0;
        else guardRoll+=gy*std::min(dt,50u)/1000.f;
        if (fabsf(guardRoll)>44) { guardSet=true; guardAt=now; guardRoll=0; }
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
    if (!validMove || !timely) { tell("符文未成形，再试一次"); return; }
    if (invoke) {
        if (scene==LinkRune) {
            shieldSlot=slot; shieldCode=slots[slot]; enter(Runes);
            tell("此机关将响应你的成功格挡"); return;
        }
        bool sent=device.sendIR(slots[slot]);
        if (sent) { enter(Runes); device.play(VictoryTone); tell("机关指令已发出"); }
        else tell("红外发射失败");
        return;
    }
    lastCast=now;
    if (scene==Training) {
        pet.train(selected); effect(selected==Frost ? Freeze : selected==Flame ? Burn : selected==Ward ? Guard : Heal);
        tell(pet.practice[selected]==3 ? "法术已精通" : "学会了一笔符文"); savePet();
    } else if (scene==Solo) {
        trial.resolve(selected,power,true,false); effect(trial.effect); finish(trial,0);
    } else if (scene==Online) match.cast(selected,power,now);
}
void startRune() {
    if (runeAction==3 && shieldSlot==slot) {
        shieldSlot=-1; shieldCode=0; tell("格挡联动已关闭"); return;
    }
    if (runeAction!=0 && !remoteCode(slots[slot])) { tell("先学习这个机关"); return; }
    if (runeAction==1 || runeAction==3) {
        if (!pet.runes) { tell("击败裂隙龙，获得现实符文"); return; }
        enter(runeAction==1 ? Invoke : LinkRune); return;
    }
    enter(runeAction==0 ? Learn : Challenge);
    if (!device.listen()) { enter(Runes); tell("红外接收启动失败"); }
}
void serviceRune() {
    uint32_t code;
    if (device.readIR(code) && remoteCode(code)) {
        if (scene==Learn) {
            bool saved=storeOK && storage.putUInt(slotKeys[slot],code)==sizeof(code);
            if (saved) {
                if (slot==shieldSlot && slots[slot]!=code) shieldSlot=-1;
                slots[slot]=code;
            }
            enter(Runes); tell(saved ? "机关绑定成功" : "机关保存失败"); device.play(Chirp);
        } else if (code==slots[slot]) {
            enter(Runes); pet.xp+=3; tell("机关挑战成功"); savePet(); device.play(VictoryTone);
        } else tell("符文不匹配，继续尝试");
    } else if (now-sceneAt>=9000) { enter(Runes); tell("接收超时"); }
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
    if (scene==Online && (!match.myTurn() || match.phase!=Prepare || match.signal!=Idle)) return;
    if (scene==Solo && hasFx && now-lastFx<1400) return;
    bool physical=scene==Invoke || scene==Reward || scene==LinkRune;
    if (physical) {
        if (bTap && !casting && scene!=LinkRune) slot=(slot+1)%3;
        if (aDown && !remoteCode(slots[slot])) { tell("此槽为空，长按 B 回巢绑定"); return; }
    } else if (bTap && !casting) selected=Skill((selected+1)%4);
    if (aDown) beginCast(!physical);
    if (casting && aUp) release();
}
void render() {
    View v;
    char line[100]={}, details[100]={}, title[64]={};
    v.title=title; v.action=line; v.hint=details;
    v.notice=now-noteAt<noteLife ? notice : "";
    v.scene=scene; v.skill=selected; v.level=pet.level(); v.evolved=pet.level()>=2;
    v.evolving=evolutionAt && now-evolutionAt<1700;
    v.onBeat=(now-castAt)%600<85 || (now-castAt)%600>515;
    v.rivalDragon=scene==Online && match.battle.mode==Duel;
    v.incoming=v.rivalDragon && match.phase==Feedback && match.myTurn();
    if (v.rivalDragon && match.phase==Feedback) v.skill=match.skill;
    v.charging=casting; v.energy=casting ? energy() : 0; v.asleep=scene==Nest && sleepPet;
    v.effect=fx; v.fxAge=hasFx ? now-lastFx : 9999;
    switch(scene) {
        case Nest:
            snprintf(title,sizeof(title),"%s / XP %lu",pet.wins ? "守隙者" : "初生", (unsigned long)pet.xp);
            snprintf(line,sizeof(line),"%s",sleepPet ? "小龙睡着了" : menus[menu]);
            v.hint=sleepPet ? "摇一摇或按键唤醒" : "B 切换   A 进入"; break;
        case Runes: case Learn: case Challenge: case Invoke: case Reward: case LinkRune: {
            snprintf(title,sizeof(title),"现实符文 ×%lu",(unsigned long)pet.runes);
            const char *actions[]={"学习","施放","挑战",slot==shieldSlot ? "解除联动" : "格挡联动"};
            snprintf(line,sizeof(line),"机关 %d  /  %s  /  %s",slot+1,remoteCode(slots[slot]) ? "已绑定" : "空槽",
                scene==Runes ? actions[runeAction] : scene==Reward ? "通关奖励" : scene==Learn ? "学习" : scene==Challenge ? "挑战" : scene==LinkRune ? "格挡联动" : "施放");
            v.hint=scene==Runes ? "B 切换   A 选择   长 B 回巢" :
                scene==Learn || scene==Challenge ? "对准魔杖，按遥控器按键" :
                scene==LinkRune ? "按住 A 挥杖，确认格挡联动" : "B 选槽   按住 A 挥杖后松开";
            break;
        }
        case Search:
            v.title="寻找房间";
            if (fresh(roomIndex)) snprintf(line,sizeof(line),"%s  #%04X",rooms[roomIndex].advert.data.mode==Duel ? "决斗" : "副本",unsigned(rooms[roomIndex].advert.data.room & 65535));
            else v.action="正在寻找附近的魔杖";
            v.hint="B 换房   A 加入   长 B 回巢"; break;
        case Hosting:
            snprintf(title,sizeof(title),"房间 #%04X",unsigned(match.room & 65535));
            v.action=match.leader ? "等待另一位玩家加入" : "正在加入房间";
            v.hint="长按 B 返回巢穴"; break;
        default: {
            const bool online=scene==Online, training=scene==Training;
            const Battle &b=online ? match.battle : trial;
            if (training) {
                v.title="符文训练";
                snprintf(line,sizeof(line),"%s  精通 %u/3",skills[selected],unsigned(pet.practice[selected]));
            } else {
                v.title=b.mode==Duel ? "面对面决斗" : b.mode==Raid ? "双人裂隙龙" : "试炼 / 裂隙龙";
                int me=online && !match.leader ? 1 : 0;
                v.hp=b.hp[me]; v.enemy=b.mode==Duel ? b.hp[1-me] : b.boss;
                v.enemyMax=b.mode==Raid ? 145 : 100; if (b.mode==Raid) v.ally=b.hp[1-me];
                if (rewarded) {
                    v.action=won ? "胜利 · 小龙获得了经验" : "小龙力竭，试炼结束";
                    v.hint="A 继续   长 B 回巢"; break;
                }
                if (online && match.phase==Aim) {
                    v.action=match.myTurn() ? "对准对方魔杖" : "静默判定 · 翻腕格挡";
                    v.hint="等待红外与无线结算"; break;
                }
                if ((online && match.phase==Feedback) || (!online && hasFx && now-lastFx<1400)) {
                    v.action=effects[b.effect]; v.hint="结算中"; break;
                }
                if (online && !match.myTurn()) {
                    v.action=b.mode==Duel ? "对手行动中" : "队友行动中";
                    v.hint=b.mode==Duel ? "对手将出招，可翻腕格挡" : "等待队友施法"; break;
                }
                snprintf(line,sizeof(line),"%s  %s",skills[selected],b.mode==Duel ? "你的回合" : b.omen()==Frost ? "敌相 / 冰" : "敌相 / 火");
            }
            if (casting) {
                snprintf(line,sizeof(line),"%s  %d%%  %s",skills[selected],v.energy,motion.matches(selected) ? "可释放" : moves[selected]);
                v.hint="发声蓄力   松开 A 释放";
            } else snprintf(details,sizeof(details),"B 换技  按 A %s  长 B 回巢",moves[selected]);
            break;
        }
    }
    draw(v,now);
}
}
void setup() {
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
        if (now-castAt>5500) { stopCast(); tell("蓄力消散，再次按 A"); }
    }
    device.updateAudio(millis());
    if (now-lastRender>=50) { render(); lastRender=now; }
    delay(2);
}

