#include "display.h"
#include <M5Unified.h>
#include <cmath>

namespace rift {
namespace {
M5Canvas canvas(&M5.Display);
const uint16_t ink=0x0865, cliff=0x19aa, teal=0x5734, pale=0xdf9b, gold=0xfdaa;
const uint16_t colors[]={0xfd88,0x6e9f,0xff50,0x8f38};
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
void sprite(int x,int y,bool asleep,bool evolved,bool boss) {
    for (int row=0;row<14;++row) {
        for (int col=0;dragon[row][col];++col) {
            char c=dragon[row][col];
            if (c==' ') continue;
            uint16_t color=c=='b' ? (boss ? 0x9a97 : evolved ? 0x8c9f : teal)
                : c=='g' ? gold : c=='c' ? pale : c=='e' ? ink : 0xffff;
            if (asleep && (c=='h' || c=='.' || c=='e')) color=c=='e' ? ink : teal;
            int scale=boss ? 2 : 3;
            int xx=x+(boss ? 19-col : col)*scale;
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
    canvas.setColorDepth(16);
    canvas.createSprite(240,135);
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextWrap(false);
}
void draw(const View &v,uint32_t now) {
    canvas.fillSprite(ink);
    for (int i=0;i<39;++i) {
        int x=(i*73+17)%240, y=24+(i*41)%68;
        canvas.drawPixel(x,y,((now/500+i)%7==0) ? pale : cliff);
    }
    canvas.drawFastHLine(8,20,224,cliff);
    textAt(8,3,"裂隙 / RIFT",teal);
    canvas.setTextColor(pale);
    canvas.drawRightString(v.title,232,3);
    int shake=v.fxAge<260 && (v.effect==Burn || v.effect==Freeze) ? int(now/42%5)-2 : 0;
    int bob=v.asleep ? 4 : int(sinf(now/330.f)*2);
    canvas.fillEllipse(60,88,38,3,cliff);
    sprite(28+(v.incoming ? shake : 0),43+bob,v.asleep,v.evolved,false);
    if (v.evolving) {
        for (int i=0;i<3;++i) {
            int radius=8+(now/25+i*12)%36;
            canvas.drawCircle(58,63,radius,i==0 ? gold : teal);
        }
    }
    if (v.asleep) textAt(83,36,"z Z",pale);
    if (v.enemy>=0) {
        if (v.rivalDragon) sprite(162+(v.incoming ? 0 : shake),42-bob,false,true,false);
        else sprite(164-shake,54-bob,false,false,true);
        bar(14,27,85,v.hp,100,teal);
        bar(141,27,85,v.enemy,v.enemyMax,0xfa70);
        if (v.ally>=0) bar(14,33,85,v.ally,100,0x83ff);
    } else {
        canvas.setTextColor(gold); canvas.setCursor(171,60); canvas.printf("Lv.%d",v.level);
    }
    if (v.charging) {
        int r=21+v.energy/12;
        canvas.drawCircle(117,63,r,colors[v.skill]);
        if (v.onBeat) canvas.drawCircle(117,63,r+2,pale);
        canvas.drawTriangle(117,48,103,72,131,72,colors[v.skill]);
        bar(88,88,60,v.energy,100,colors[v.skill]);
    } else if (v.fxAge<1100) {
        int step=v.fxAge/22;
        if (v.effect==Block || v.effect==Guard) {
            int shieldX=v.incoming ? 92 : 137;
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
    canvas.fillRect(0,95,240,40,0x1107);
    textAt(8,98,*v.notice ? v.notice : v.action,*v.notice ? gold : 0xffff);
    textAt(8,117,v.hint,0xa556);
    canvas.pushSprite(0,0);
}
}

