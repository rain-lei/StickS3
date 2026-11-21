#pragma once

#include <M5GFX.h>
#include <stdint.h>

namespace rift { namespace ui {

enum class Mode : uint8_t { Wand, Training, Trial, Duel, CoOp, Mechanism, Lobby, Preview };
enum class Skill : uint8_t { FireSlash, IceArrow, Shield, Heal };
enum class Status : uint8_t {
  WaitingForConnection, Ready, Gesture, Charging, Release, Judging,
  WaitingForPlayer, Hit, TookHit, Blocked, CounterReady, Miss, Frozen,
  Healed, RetryGesture, Victory, Defeat, Disconnected, LearnRemote,
  RemoteLearned, LearnTimeout, AimAtDevice, SignalSent, Selection, Notice,
  OtherBlocked, GuardRaised, Success
};

struct Prompt { const char* title; const char* hint; };

struct BoardView {
  Mode mode = Mode::Wand;
  Status status = Status::WaitingForConnection;
  Skill skill = Skill::IceArrow;
  uint8_t chargePercent = 0;
  bool onBeat = false;
  uint8_t slot = 0;  // 1–3 for a known mechanism, 0 when there is no selection.
  char title[72] = {};
  char hint[80] = {};
  char detail[32] = {};  // Short top-right label, e.g. a room number or slot.
  void setPrompt(const char* headline, const char* nextAction = "");
  Prompt prompt() const;
};

class BoardUi {
 public:
  explicit BoardUi(lgfx::LGFXBase& display) : display_(display) {}
  bool show(const BoardView& view);  // True if pixels changed.
  void invalidate() { painted_ = false; }  // After waking or clearing the display.

 private:
  lgfx::LGFXBase& display_;
  BoardView last_;
  bool painted_ = false;
};

}}  // namespace rift::ui
