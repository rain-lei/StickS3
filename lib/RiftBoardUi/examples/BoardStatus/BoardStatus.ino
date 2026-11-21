#include <M5Unified.h>
#include <RiftBoardUi.h>

using rift::ui::Skill;
using rift::ui::Status;

rift::ui::BoardUi screen(M5.Display);
rift::ui::BoardView view;

constexpr Skill skills[] = {Skill::FireSlash, Skill::IceArrow, Skill::Shield, Skill::Heal};
constexpr Status samples[] = {
  Status::WaitingForConnection, Status::Ready, Status::Gesture, Status::Charging,
  Status::Release, Status::Judging, Status::Hit, Status::Blocked, Status::CounterReady,
  Status::Miss, Status::TookHit, Status::Frozen, Status::Healed, Status::RetryGesture,
  Status::WaitingForPlayer, Status::Victory, Status::Defeat, Status::Disconnected,
  Status::LearnRemote, Status::RemoteLearned, Status::LearnTimeout,
  Status::AimAtDevice, Status::SignalSent
};

size_t skillIndex = 1;
size_t sampleIndex = 0;
bool browsing = false;
bool holding = false;
uint32_t pressedAt = 0;
uint32_t releasedAt = 0;

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) % 4);
  }
  M5.BtnB.setHoldThresh(650);
  view.mode = rift::ui::Mode::Preview;
  view.status = Status::Ready;
  screen.show(view);
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  if (M5.BtnB.wasHold()) {
    browsing = !browsing;
    holding = false;
    sampleIndex = 0;
    view.status = browsing ? samples[0] : Status::Ready;
  } else if (browsing) {
    if (M5.BtnA.wasClicked()) {
      browsing = false;
      view.status = Status::Ready;
    } else if (M5.BtnB.wasClicked()) {
      sampleIndex = (sampleIndex + 1) % (sizeof(samples) / sizeof(samples[0]));
      view.status = samples[sampleIndex];
    }
    view.slot = 2;
    view.chargePercent = 70;
  } else {
    if (M5.BtnB.wasClicked() && !holding) {
      skillIndex = (skillIndex + 1) % (sizeof(skills) / sizeof(skills[0]));
      view.skill = skills[skillIndex];
    }
    if (M5.BtnA.wasPressed()) {
      holding = true;
      pressedAt = now;
      view.status = Status::Gesture;
    }
    if (holding && M5.BtnA.isPressed()) {
      // Elapsed time drives this UI preview only. The game supplies measured charge.
      const uint32_t elapsed = now - pressedAt;
      view.onBeat = elapsed % 600 < 85 || elapsed % 600 > 515;
      if (elapsed >= 1500) {
        view.status = Status::Release;
      } else if (elapsed >= 350) {
        view.status = Status::Charging;
        view.chargePercent = static_cast<uint8_t>((elapsed - 350) * 100 / 1150);
      }
    }
    if (holding && M5.BtnA.wasReleased()) {
      holding = false;
      releasedAt = now;
      view.status = Status::Judging;
    } else if (view.status == Status::Judging && now - releasedAt >= 650) {
      view.status = Status::Ready;
    }
  }
  screen.show(view);
  delay(5);
}
