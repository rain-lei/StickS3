#include "display.h"
#include <M5Unified.h>
#include <cmath>

namespace rift {
namespace {
M5Canvas canvas(&M5.Display);
ui::BoardUi directUi(M5.Display);
bool buffered=false;
constexpr uint16_t ink=0x1083, panel=0x18c5, cliff=0x39a9, lilac=0xc55f;
constexpr uint16_t pale=0xef9f, muted=0xad59, gold=0xff31;
const uint16_t colors[]={0xfdce,0x75df,0xcd1f,0x8f74};
enum Artwork { Frostling, BossArt, DuelArt, Portal, Shrine, FireArt, FrostArt, WardArt, BloomArt };
const Artwork spells[]={FireArt,FrostArt,WardArt,BloomArt};
constexpr int tileW=112, tileH=68;
extern "C" const lgfx::swap565_t tiles[] asm("_binary_assets_rift_tiles_bin_start");

void portrait(Artwork art,int x,int y,int dx=0,int dy=0) {
    canvas.setClipRect(x,y,tileW,tileH);
    canvas.pushImage(x+dx,y+dy,tileW,tileH,tiles+art*tileW*tileH);
    canvas.clearClipRect();
}
const char *dragon[]={
    "      g        g    ",
    "      gg      gg    ",
    "      bgg  bbggb    ",
    " b    bbggbbbbbb    ",
    " bb    bbbbbbhhbb   ",
    " bbg   bbbbbbh.ebb  ",
    " bbbg bbbbbbbbbbbbb ",
    "  bbbgbbbbccccbbbbb ",
    "   bbbbbbbccccccbb  ",
    "g  bbbbbbcccccb     ",
    "bg bbbbbbccccbb     ",
    " bbbbbbbbcccb       ",
    "  bbbbbbbbbbb       ",
    "    bbbb  bbbb      "
};
void sprite(int x,int y,bool asleep,bool evolved,bool rival) {
    for (int row=0;row<14;++row) {
        for (int col=0;dragon[row][col];++col) {
            char c=dragon[row][col];
            if (c==' ') continue;
            uint16_t color=c=='b' ? (rival ? 0xcadd : evolved ? 0x8c9f : 0x5734)
                : c=='g' ? gold : c=='c' ? pale : c=='e' ? ink : 0xffff;
            if (asleep && (c=='h' || c=='.' || c=='e')) color=c=='e' ? ink : 0x5734;
            int scale=2;
            int xx=x+(rival ? 19-col : col)*scale;
            canvas.fillRect(xx,y+row*scale,scale,scale,color);
        }
    }
}
void textAt(int x,int y,const char *s,uint16_t color) {
    canvas.setTextColor(color); canvas.drawString(s,x,y);
}
void bar(int x,int y,int width,int n,int max,uint16_t color) {
    canvas.fillRect(x,y,width,3,cliff);
    canvas.fillRect(x,y,width*n/max,3,color);
}
}
void initDisplay() {
    M5.Display.setRotation(1);
    if (M5.Display.width()<M5.Display.height())
        M5.Display.setRotation((M5.Display.getRotation()+1)%4);
    canvas.setColorDepth(16);
    buffered=canvas.createSprite(240,135)!=nullptr;
    directUi.invalidate();
    if (!buffered) return;
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextWrap(false);
}
void draw(const View &v,uint32_t now) {
    if (!buffered) { directUi.show(v.status); return; }
    canvas.fillSprite(ink);
    canvas.fillTriangle(7,10,15,3,13,16,lilac);
    canvas.fillTriangle(13,16,18,7,16,14,0x935d);
    textAt(23,3,"RIFT",pale);
    canvas.setTextColor(muted);
    canvas.drawRightString(v.title,233,3);
    canvas.drawFastHLine(6,20,228,cliff);
    int bob=v.asleep ? 2 : int(sinf(now/330.f)*2);
    if (v.enemy>=0) {
        int shake=v.fxAge<260 && (v.effect==Burn || v.effect==Freeze) ? int(now/42%5)-2 : 0;
        portrait(Frostling,6,25,v.incoming ? shake : 0,bob);
        portrait(v.rivalDragon ? DuelArt : BossArt,122,25,v.incoming ? 0 : shake);
        int lunge=v.fxAge<340 ? int(sinf(v.fxAge*0.0092f)*9) : 0;
        sprite(30+(v.incoming ? shake : lunge),59+bob,false,v.evolved,false);
        sprite(170-(v.incoming ? lunge : shake),59-bob,false,false,true);
        if (v.rivalDragon) {
            for (int i=0;i<v.layers;++i) canvas.drawRoundRect(72+4*i,47-i*2,8,37+i*4,4,lilac);
            for (int i=0;i<v.enemyLayers;++i) canvas.drawRoundRect(157-4*i,47-i*2,8,37+i*4,4,lilac);
            if (v.flight>=0) {
                int x=65+(now/8)%105, dir=v.hostileShot ? -1 : 1;
                if (v.hostileShot) x=240-x;
                canvas.fillTriangle(x+8*dir,67,x-7*dir,63,x-7*dir,71,colors[v.flight]);
                canvas.drawFastHLine(x-(dir>0 ? 19 : -7),67,12,pale);
            }
            if (v.enemyCharging) canvas.drawCircle(188,66,14+(now/70)%8,colors[v.enemyCharging-1]);
        }
        canvas.fillRect(6,24,112,9,panel);
        canvas.fillRect(122,24,112,9,panel);
        bar(9,25,106,v.hp,100,colors[Bloom]);
        bar(125,25,106,v.enemy,v.enemyMax,colors[Flame]);
        if (v.ally>=0) bar(9,30,106,v.ally,100,colors[Frost]);
    } else {
        Artwork art=Shrine;
        const char *headline="现实机关";
        char caption[40]={};
        uint16_t accent=lilac;
        switch (v.scene) {
            case Nest:
                art=Frostling;
                headline="霜芽";
                accent=v.evolved ? gold : lilac;
                snprintf(caption,sizeof(caption),"Lv.%d   %d/30 XP",v.level,v.levelXp);
                bar(9,85,96,v.levelXp,30,accent);
                break;
            case Training: {
                art=spells[v.skill]; headline=v.skillName; accent=colors[v.skill];
                snprintf(caption,sizeof(caption),"法术精通  %d/3",v.mastery);
                for (int i=0;i<3;++i) canvas.fillRect(9+i*33,85,29,3,i<v.mastery ? accent : cliff);
                break;
            }
            case Hosting:
                art=v.roomMode==Duel ? DuelArt : Portal;
                headline=v.roomMode==Duel ? "双人决斗" : "协作副本";
                break;
            case Search:
                art=Portal; headline="联机房间"; break;
            default:
                break;
        }
        portrait(art,122,25,0,v.scene==Nest ? bob : 0);
        canvas.setTextSize(2);
        textAt(8,35,headline,pale);
        canvas.setTextSize(1);
        if (*caption) textAt(9,65,caption,accent);
        if (v.scene==Nest) sprite(126,62+bob,v.asleep,v.evolved,false);
        if (v.asleep) textAt(217,28,"zZ",pale);
    }
    if (v.evolved && (v.scene==Nest || v.enemy>=0)) {
        int x=v.scene==Nest ? 130 : 14;
        canvas.fillTriangle(x-5,38,x,42,x+5,38,gold);
        canvas.fillTriangle(x-3,42,x,35,x+3,42,gold);
    }
    if (v.evolving) {
        int x=v.enemy>=0 ? 62 : 181;
        for (int i=0;i<3;++i) canvas.drawCircle(x,57,8+(now/25+i*12)%27,i ? lilac : gold);
    }
    if (v.charging) {
        int r=21+v.energy/12;
        const int x=v.scene==Training ? 178 : 117;
        canvas.drawCircle(x,63,r,colors[v.skill]);
        if (v.onBeat) canvas.drawCircle(x,63,r+2,pale);
        if (v.scene!=Training) canvas.drawTriangle(x,48,x-14,72,x+14,72,colors[v.skill]);
        bar(x-30,88,60,v.energy,100,colors[v.skill]);
    }
    if (v.fxAge<1100) {
        int step=v.fxAge/22;
        if (v.effect==Block || v.effect==Guard) {
            int shieldX=v.incoming ? 82 : 148;
            if (v.effect==Block) for (int i=0;i<7;++i) {
                int side=v.incoming ? 1 : -1;
                int tip=shieldX+(v.incoming ? 18 : -2);
                int tail=tip+side*(19+(i*7+step)%24);
                int y=53+i*4;
                canvas.fillTriangle(tip,y,tail,y-4,tail,y+3,colors[v.skill]);
            }
            canvas.drawRoundRect(shieldX,41,16,45,7,gold);
            canvas.drawRoundRect(shieldX+2,44,12,39,5,pale);
        } else if (v.effect==Burn || v.effect==Freeze || v.effect==Heal) {
            uint16_t c=v.effect==Burn ? colors[Flame] : v.effect==Freeze ? colors[Frost] : colors[Bloom];
            for(int i=0;i<18;++i) {
                int x=82+(i*7+step*4)%113, y=62+(i*13+step)%19-9;
                if (v.incoming) x=240-x;
                canvas.drawLine(x,y,x+4+i%5,y-2,c);
            }
            if (v.effect==Freeze) for(int i=0;i<8;++i) {
                int x=(v.incoming ? 8 : 144)+i*11;
                canvas.drawTriangle(x,33,x+6,44+i%3*4,x+10,33,c);
            }
        }
    }
    canvas.fillRect(0,95,240,40,panel);
    const auto prompt=v.status.prompt();
    canvas.fillRect(0,97,3,16,v.hasNotice ? gold : lilac);
    canvas.setFont(&fonts::efontCN_16);
    if (canvas.textWidth(prompt.title)>224) canvas.setFont(&fonts::efontCN_12);
    textAt(8,96,prompt.title,v.hasNotice ? gold : 0xffff);
    canvas.setFont(&fonts::efontCN_12);
    if (v.rivalDragon && v.hp>0 && v.enemy>0 && !v.hasNotice) {
        static const char *names[]={"火","冰","盾","愈"};
        for (int i=0;i<4;++i) {
            int x=6+i*58;
            uint16_t color=v.cooldown[i] ? muted : colors[i];
            if (i==v.skill) canvas.drawRoundRect(x,114,54,20,3,color);
            textAt(x+4,117,names[i],color);
            char label[12];
            if (v.cooldown[i]) snprintf(label,sizeof(label),"%u.%us",unsigned(v.cooldown[i]/1000),unsigned(v.cooldown[i]/100%10));
            else snprintf(label,sizeof(label),"就绪");
            textAt(x+20,117,label,color);
        }
    } else textAt(8,117,prompt.hint,muted);
    canvas.pushSprite(0,0);
}
}

