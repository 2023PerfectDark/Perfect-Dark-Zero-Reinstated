#pragma once
/**
 * @file        rex/input/mnk/mnk_stick.h
 * @brief       Keyboard and mouse to stick math of the MnK input driver: the
 *              key ramp of the left stick, the mouse speed of the right stick
 *              and modifier keys bound on their own
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ForzaHorizon1-ReImagined SDK
 *              patch 0040.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rex::input::mnk {

// A key moves a ramped stick axis from 0 to this at once (a short tap must
// clear the game's dead zone), then to full over the ramp.
constexpr float kStickRampStart = 0.6f;

// Moves a stick axis (-1..1) toward target (-1, 0 or 1) over dt seconds: to
// full over ramp seconds, and back to 0 (or through it to the other side) in
// half of that. A ramp of 0 or less jumps to the target. Returns the new value.
inline float RampStickAxis(float& value, float target, float dt, float ramp) {
  if (ramp <= 0.0f) {
    value = target;
    return value;
  }
  const float attack = dt / ramp;
  const bool releasing = target == 0.0f || (value != 0.0f && (target > 0) != (value > 0));
  if (releasing) {
    const float next = std::fabs(value) - 2.0f * attack;
    value = (next > 0.0f ? next : 0.0f) * (value < 0 ? -1.0f : 1.0f);
    if (target == 0.0f || value != 0.0f) {
      return value;
    }
  }
  float magnitude = std::max(std::fabs(value), kStickRampStart);
  magnitude = std::min(1.0f, magnitude + attack);
  value = target < 0 ? -magnitude : magnitude;
  return value;
}

// Low-passes the mouse speed (pixels a second) over the polls with a 50 ms
// time constant: a game may poll twice in a frame, and the second poll sees no
// motion. delta is the motion since the last poll, dt its seconds.
inline void UpdateMouseRate(float& rate, float delta, float dt) {
  if (dt <= 0.0f) {
    return;
  }
  constexpr float kTau = 0.05f;
  const float alpha = 1.0f - std::exp(-dt / kTau);
  rate += (delta / dt - rate) * alpha;
}

// Right stick units per pixel a second at sensitivity 1: 1000 px/s is half the
// stick.
constexpr double kMouseRateScale = 16.0;

// Modifier bits of a bind ("Shift+W") and of the keys held.
constexpr uint8_t kModShift = 1u << 0;
constexpr uint8_t kModCtrl = 1u << 1;
constexpr uint8_t kModAlt = 1u << 2;

// One alternative of a bind ("Shift+W"): a key (ui::VirtualKey, below 256)
// and the modifiers it needs.
struct BindToken {
  uint16_t key;
  uint8_t mods;
};

// Which binds fire for the keys held. A bind fires while its key and every
// modifier it needs are held, whatever other modifiers are held too (Shift to
// sprint and W to move). A combination that is held ("Shift+Up") claims its
// key from the plain binds of it ("Up" stays silent while Shift+Up is held),
// which lets the D-pad share the arrow keys. A modifier bound on its own
// ("Shift") fires like any key.
class BindClaims {
 public:
  BindClaims(const bool (&key_down)[256], uint8_t live_mods)
      : key_down_(key_down), live_mods_(live_mods) {}

  // Every alternative of every bind goes through this first.
  void Claim(BindToken token) {
    if (token.mods && Held(token)) {
      claimed_[token.key] = true;
    }
  }

  bool Fires(BindToken token) const { return Held(token) && (token.mods || !claimed_[token.key]); }

 private:
  bool Held(BindToken token) const {
    return token.key < 256 && key_down_[token.key] && (token.mods & live_mods_) == token.mods;
  }

  const bool (&key_down_)[256];
  uint8_t live_mods_;
  bool claimed_[256] = {};
};

}  // namespace rex::input::mnk
