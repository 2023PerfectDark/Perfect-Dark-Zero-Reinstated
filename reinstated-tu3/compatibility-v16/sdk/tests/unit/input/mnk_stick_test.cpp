/**
 * @file        input/mnk_stick_test.cpp
 * @brief       Tests for the keyboard and mouse to stick math of the MnK driver
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <rex/input/mnk/mnk_stick.h>

using Catch::Approx;
using namespace rex::input::mnk;

TEST_CASE("Without a ramp the keys push the stick to full at once", "[input][mnk]") {
  float value = 0.0f;
  CHECK(RampStickAxis(value, 1.0f, 0.016f, 0.0f) == 1.0f);
  CHECK(RampStickAxis(value, -1.0f, 0.016f, 0.0f) == -1.0f);
  CHECK(RampStickAxis(value, 0.0f, 0.016f, 0.0f) == 0.0f);
}

TEST_CASE("A ramped key starts past the dead zone and reaches full", "[input][mnk]") {
  float value = 0.0f;
  // 0.2 s ramp, polls 50 ms apart: a quarter of the ramp each.
  CHECK(RampStickAxis(value, 1.0f, 0.05f, 0.2f) == Approx(kStickRampStart + 0.25f));
  CHECK(RampStickAxis(value, 1.0f, 0.05f, 0.2f) == 1.0f);
  CHECK(RampStickAxis(value, 1.0f, 0.05f, 0.2f) == 1.0f);
}

TEST_CASE("Letting go takes half the ramp", "[input][mnk]") {
  float value = 1.0f;
  CHECK(RampStickAxis(value, 0.0f, 0.05f, 0.2f) == Approx(0.5f));
  CHECK(RampStickAxis(value, 0.0f, 0.05f, 0.2f) == 0.0f);
}

TEST_CASE("The other key first lets go, then pushes the other way", "[input][mnk]") {
  float value = 1.0f;
  CHECK(RampStickAxis(value, -1.0f, 0.05f, 0.2f) == Approx(0.5f));
  CHECK(RampStickAxis(value, -1.0f, 0.05f, 0.2f) == Approx(-(kStickRampStart + 0.25f)));
}

TEST_CASE("The mouse speed follows the motion and ignores a poll at the same instant",
          "[input][mnk]") {
  float rate = 0.0f;
  UpdateMouseRate(rate, 10.0f, 0.0f);
  CHECK(rate == 0.0f);
  // 1000 px/s held long enough settles at 1000 px/s.
  for (int i = 0; i < 100; ++i) {
    UpdateMouseRate(rate, 10.0f, 0.01f);
  }
  CHECK(rate == Approx(1000.0f).epsilon(0.001));
  // Half the stick at sensitivity 1.
  CHECK(rate * kMouseRateScale == Approx(16000.0).epsilon(0.001));
}

namespace {

// Virtual key codes as ui::VirtualKey numbers them (Windows VK_).
constexpr uint16_t kKeyShift = 0x10;
constexpr uint16_t kKeyUp = 0x26;
constexpr uint16_t kKeyW = 'W';

}  // namespace

TEST_CASE("A plain bind fires while an unrelated modifier is held", "[input][mnk]") {
  bool key_down[256] = {};
  key_down[kKeyShift] = true;
  key_down[kKeyW] = true;
  BindClaims claims(key_down, kModShift);
  // Binds: "Shift" (sprint) and "W" (move).
  claims.Claim({kKeyShift, 0});
  claims.Claim({kKeyW, 0});
  CHECK(claims.Fires({kKeyShift, 0}));
  CHECK(claims.Fires({kKeyW, 0}));
}

TEST_CASE("A held combination claims its key from the plain binds", "[input][mnk]") {
  bool key_down[256] = {};
  key_down[kKeyUp] = true;
  {
    // Binds: "Up" (D-pad) and "Shift+Up" (stick); Shift not held.
    BindClaims claims(key_down, 0);
    claims.Claim({kKeyUp, 0});
    claims.Claim({kKeyUp, kModShift});
    CHECK(claims.Fires({kKeyUp, 0}));
    CHECK_FALSE(claims.Fires({kKeyUp, kModShift}));
  }
  key_down[kKeyShift] = true;
  {
    BindClaims claims(key_down, kModShift);
    claims.Claim({kKeyUp, 0});
    claims.Claim({kKeyUp, kModShift});
    CHECK_FALSE(claims.Fires({kKeyUp, 0}));
    CHECK(claims.Fires({kKeyUp, kModShift}));
  }
}
