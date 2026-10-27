/**
 * @file        graphics/display_transfer_test.cpp
 * @brief       Tests for the BT.1886 display output transfer of the gamma ramps
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>

#include <rex/graphics/util/display_transfer.h>

using namespace rex::graphics;

TEST_CASE("BT.1886 to sRGB keeps black and white", "[graphics][display]") {
  CHECK(display_transfer::Bt1886ToSrgb(0.0) == 0.0);
  CHECK(display_transfer::Bt1886ToSrgb(1.0) == Catch::Approx(1.0));
  // Out of range values are clamped.
  CHECK(display_transfer::Bt1886ToSrgb(-0.5) == 0.0);
  CHECK(display_transfer::Bt1886ToSrgb(1.5) == Catch::Approx(1.0));
  CHECK(display_transfer::Bt1886ToSrgb10(0) == 0);
  CHECK(display_transfer::Bt1886ToSrgb10(1023) == 1023);
}

TEST_CASE("BT.1886 to sRGB matches the exact curve", "[graphics][display]") {
  for (uint32_t i = 0; i <= 1023; ++i) {
    const double v = i / 1023.0;
    // Decode with gamma 2.4, encode with the sRGB curve.
    const double linear = std::pow(v, 2.4);
    const double srgb =
        linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
    CHECK(display_transfer::Bt1886ToSrgb(v) == Catch::Approx(srgb).margin(1e-9));
  }
  // Mid grey gets darker: an HDTV's 2.4 is darker than a PC display's sRGB.
  CHECK(display_transfer::Bt1886ToSrgb(0.5) == Catch::Approx(0.4725));
  uint32_t previous = 0;
  for (uint32_t i = 0; i <= 1023; ++i) {
    const uint32_t converted = display_transfer::Bt1886ToSrgb10(i);
    CHECK(converted >= previous);
    CHECK(converted <= i);
    previous = converted;
  }
}

TEST_CASE("BT.1886 PWL segments stay continuous", "[graphics][display]") {
  // A linear ramp in 128 segments: each ends where the next starts.
  constexpr uint32_t kStep = 65472 / 128;
  reg::DC_LUT_PWL_DATA converted[128];
  for (uint32_t i = 0; i < 128; ++i) {
    reg::DC_LUT_PWL_DATA raw = {};
    raw.base = i * kStep;
    raw.delta = kStep;
    converted[i] = display_transfer::Bt1886ToSrgbPwl(raw);
  }
  CHECK(converted[0].base == 0);
  for (uint32_t i = 0; i + 1 < 128; ++i) {
    const int32_t end = int32_t(converted[i].base + converted[i].delta);
    CHECK(std::abs(end - int32_t(converted[i + 1].base)) <= 1);
  }
  CHECK(converted[127].base + converted[127].delta >= 65472 - kStep / 2);
}
