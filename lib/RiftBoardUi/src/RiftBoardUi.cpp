#include "RiftBoardUi.h"
#include <cstdio>
#include <cstring>

namespace rift { namespace ui {
namespace {

constexpr uint16_t rgb565(uint32_t rgb) {
  return ((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f);
}
constexpr uint16_t bg = rgb565(0x101827), surface = rgb565(0x1b293b);
constexpr uint16_t fg = rgb565(0xedf4f2), muted = rgb565(0xb0bfcb);
constexpr uint16_t track = rgb565(0x334257), ice = rgb565(0x83ddf7);
constexpr uint16_t fire = rgb565(0xff9174), leaf = rgb565(0x89e4b5);
constexpr uint16_t gold = rgb565(0xffd384);

enum class Mark { Rune, Slash, Arrow, Shield, Cross, Wait, Check, Miss, Frost, Alert, Target };

struct Cue {
  const char* title;
  const char* hint;
  Mark mark;
  uint16_t color;
};

struct Spell {
  const char* name;
  const char* gesture;
  Mark mark;
  uint16_t color;
};

Spell spellFor(Skill skill) {
  switch (skill) {
    case Skill::FireSlash: return {"火斩", "横向挥动", Mark::Slash, fire};
    case Skill::IceArrow:  return {"冰箭", "向前推", Mark::Arrow, ice};
    case Skill::Shield:    return {"护盾", "翻转手腕", Mark::Shield, gold};
    case Skill::Heal:      return {"治愈", "画一个圈", Mark::Cross, leaf};
  }
  return {"", "做出动作", Mark::Rune, muted};
}

const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::Wand:      return "魔杖";
    case Mode::Training:  return "训练";
    case Mode::Trial:     return "试炼";
    case Mode::Duel:      return "决斗";
    case Mode::CoOp:      return "共斗";
    case Mode::Mechanism: return "机关";
    case Mode::Lobby:     return "联机";
    case Mode::Preview:   return "界面预览";
  }
  return "魔杖";
}

bool showsSkill(Status status) {
  return status == Status::Ready || status == Status::Gesture ||
         status == Status::Charging || status == Status::Release ||
         status == Status::RetryGesture;
}

bool showsSlot(Status status) {
  return status == Status::LearnRemote || status == Status::RemoteLearned ||
         status == Status::LearnTimeout || status == Status::AimAtDevice ||
         status == Status::SignalSent;
}

Cue cueFor(Status status, const Spell& spell) {
  switch (status) {
    case Status::WaitingForConnection: return {"等待连接", "等待对方魔杖", Mark::Wait, muted};
    case Status::Ready:                return {"按住 A", "B 切换技能", spell.mark, spell.color};
    case Status::Gesture:              return {spell.gesture, "松 A 施法", spell.mark, spell.color};
    case Status::Charging:             return {"正在蓄力", "", spell.mark, spell.color};
    case Status::Release:              return {"松开 A", "释放当前法术", spell.mark, spell.color};
    case Status::Judging:              return {"判定中", "", Mark::Wait, gold};
    case Status::WaitingForPlayer:     return {"等待对方", "", Mark::Wait, muted};
    case Status::Hit:                  return {"已命中", "", Mark::Target, spell.color};
    case Status::TookHit:              return {"受到攻击", "", Mark::Alert, fire};
    case Status::Blocked:              return {"已格挡", "", Mark::Shield, gold};
    case Status::CounterReady:         return {"可以反击", "", Mark::Slash, gold};
    case Status::Miss:                 return {"未命中", "", Mark::Miss, muted};
    case Status::Frozen:               return {"已冰冻", "", Mark::Frost, ice};
    case Status::Healed:               return {"已治愈", "", Mark::Cross, leaf};
    case Status::RetryGesture:         return {"再试一次", spell.gesture, Mark::Alert, gold};
    case Status::Victory:              return {"胜利", "A 继续", Mark::Check, gold};
    case Status::Defeat:               return {"挑战失败", "A 继续", Mark::Miss, muted};
    case Status::Disconnected:         return {"连接中断", "回灵巢重新组队", Mark::Alert, fire};
    case Status::LearnRemote:          return {"按遥控键", "对准魔杖", Mark::Target, ice};
    case Status::RemoteLearned:        return {"已学会", "", Mark::Check, leaf};
    case Status::LearnTimeout:         return {"学习超时", "请重试", Mark::Alert, gold};
    case Status::AimAtDevice:          return {"请挥杖", "对准已选设备", Mark::Arrow, ice};
    case Status::SignalSent:           return {"信号已发", "", Mark::Check, leaf};
    case Status::Selection:            return {"请选择", "", Mark::Rune, ice};
    case Status::Notice:               return {"提示", "", Mark::Alert, gold};
    case Status::OtherBlocked:         return {"对方格挡", "", Mark::Shield, muted};
    case Status::GuardRaised:          return {"已架盾", "", Mark::Shield, gold};
    case Status::Success:              return {"成功", "", Mark::Check, leaf};
  }
  return {"请稍候", "", Mark::Wait, muted};
}

Cue resolvedCue(const BoardView& v, const Spell& spell) {
  Cue cue = cueFor(v.status, spell);
  if (v.mode == Mode::Preview && v.status == Status::Ready) cue.hint = "B 换技 / 长按预览";
  if (v.title[0]) { cue.title = v.title; cue.hint = v.hint; }
  return cue;
}

void drawMark(lgfx::LGFXBase& d, Mark mark, uint16_t color) {
  constexpr int x = 44, y = 74;
  d.fillRoundRect(x-28, y-28, 57, 57, 10, surface);
  auto line = [&](int x1, int y1, int x2, int y2) {
    d.drawLine(x+x1, y+y1, x+x2, y+y2, color);
    d.drawLine(x+x1, y+y1+1, x+x2, y+y2+1, color);
  };
  switch (mark) {
    case Mark::Slash:
      line(-14, 12, 12, -14); line(-9, 15, 14, -8); break;
    case Mark::Arrow:
      line(-13, 13, 12, -12); line(0, -12, 12, -12); line(12, -12, 12, 0); break;
    case Mark::Shield:
      line(-13,-10,0,-14); line(0,-14,13,-10); line(13,-10,11,4);
      line(11,4,0,14); line(0,14,-11,4); line(-11,4,-13,-10); break;
    case Mark::Cross:
      d.fillRoundRect(x-4,y-14,9,29,2,color); d.fillRoundRect(x-14,y-4,29,9,2,color); break;
    case Mark::Wait:
      for (int i=-1; i<=1; ++i) d.fillCircle(x+i*10, y, 3, color);
      break;
    case Mark::Check:
      line(-14,0,-4,10); line(-4,10,14,-11); break;
    case Mark::Miss:
      line(-10,-10,10,10); line(-10,10,10,-10); break;
    case Mark::Frost:
      line(0,-15,0,15); line(-13,-8,13,8); line(-13,8,13,-8);
      line(-5,-12,0,-8); line(0,-8,5,-12); line(-5,12,0,8); line(0,8,5,12); break;
    case Mark::Alert:
      line(0,-14,15,12); line(15,12,-15,12); line(-15,12,0,-14);
      d.fillRect(x-1,y-5,3,9,color); d.fillCircle(x,y+7,1,color); break;
    case Mark::Target:
      d.drawCircle(x,y,14,color); d.drawCircle(x,y,8,color); d.fillCircle(x,y,3,color); break;
    case Mark::Rune:
      line(0,-14,12,0); line(12,0,0,14); line(0,14,-12,0); line(-12,0,0,-14); break;
  }
}

void drawCharge(lgfx::LGFXBase& d, uint8_t percent, bool onBeat, uint16_t color) {
  constexpr int x=19, y=116, width=202;
  const int filled = width * percent / 100;
  if (filled) d.fillRect(x, y, filled, 5, color);
  if (filled < width) d.fillRect(x+filled, y, width-filled, 5, track);
  d.fillCircle(10, y+2, 2, onBeat ? gold : track);
}

}  // namespace

void BoardView::setPrompt(const char* headline, const char* nextAction) {
  std::snprintf(title, sizeof(title), "%s", headline ? headline : "");
  std::snprintf(hint, sizeof(hint), "%s", nextAction ? nextAction : "");
}

Prompt BoardView::prompt() const {
  const Cue cue = resolvedCue(*this, spellFor(skill));
  return {cue.title, cue.hint};
}

bool BoardUi::show(const BoardView& input) {
  BoardView v = input;
  if (v.chargePercent > 100) v.chargePercent = 100;
  if (v.slot > 3 || !showsSlot(v.status)) v.slot = 0;
  if (!showsSkill(v.status) && v.status != Status::Hit) v.skill = Skill::IceArrow;
  const bool same = painted_ && v.status == last_.status &&
                    v.mode == last_.mode && v.skill == last_.skill && v.slot == last_.slot &&
                    std::strcmp(v.title, last_.title) == 0 && std::strcmp(v.hint, last_.hint) == 0 &&
                    std::strcmp(v.detail, last_.detail) == 0;
  const Spell spell = spellFor(v.skill);
  if (same) {
    const bool changed = v.status == Status::Charging &&
                         (v.chargePercent != last_.chargePercent || v.onBeat != last_.onBeat);
    if (changed) {
      display_.startWrite();
      drawCharge(display_, v.chargePercent, v.onBeat, spell.color);
      display_.endWrite();
    }
    last_ = v;
    return changed;
  }
  const Cue cue = resolvedCue(v, spell);
  auto& d = display_;
  d.startWrite();
  d.fillScreen(bg);
  d.setTextSize(1);
  d.setTextPadding(0);
  d.setFont(&lgfx::fonts::efontCN_16);
  d.setTextDatum(lgfx::textdatum_t::middle_left);
  d.setTextColor(muted, bg);
  d.drawString(modeName(v.mode), 12, 15);
  d.setTextDatum(lgfx::textdatum_t::middle_right);
  if (v.detail[0]) {
    d.drawString(v.detail, 228, 15);
  } else if (showsSkill(v.status)) {
    d.setTextColor(spell.color, bg);
    d.drawString(spell.name, 228, 15);
  } else if (v.slot != 0) {
    const char* slots[] = {"", "机关 1", "机关 2", "机关 3"};
    d.drawString(slots[v.slot], 228, 15);
  }
  d.drawFastHLine(12, 29, 216, track);
  drawMark(d, cue.mark, cue.color);
  const bool bottomHint = cue.hint[0] && v.status != Status::Charging && d.textWidth(cue.hint) > 144;
  d.setTextDatum(lgfx::textdatum_t::middle_left);
  d.setFont(&lgfx::fonts::efontCN_24);
  if (d.textWidth(cue.title) > 145) d.setFont(&lgfx::fonts::efontCN_16);
  d.setTextColor(fg, bg);
  d.drawString(cue.title, 87, cue.hint[0] && !bottomHint ? 63 : 74);
  if (cue.hint[0]) {
    d.setFont(&lgfx::fonts::efontCN_16);
    d.setTextColor(muted, bg);
    d.drawString(cue.hint, bottomHint ? 12 : 88, bottomHint ? 118 : 91);
  }
  if (v.status == Status::Charging) drawCharge(d, v.chargePercent, v.onBeat, spell.color);
  d.endWrite();
  last_ = v;
  painted_ = true;
  return true;
}

}}  // namespace rift::ui
