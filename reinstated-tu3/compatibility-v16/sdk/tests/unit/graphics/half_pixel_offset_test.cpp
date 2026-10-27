/**
 * @file        graphics/half_pixel_offset_test.cpp
 * @brief       Tests for the half-pixel offset under resolution scaling
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/util/draw.h>

using rex::graphics::draw_util::GetHalfPixelOffset;

TEST_CASE("Half-pixel offset is half a guest pixel by default", "[graphics][half_pixel]") {
  CHECK(GetHalfPixelOffset(1, false) == 0.5f);
  CHECK(GetHalfPixelOffset(2, false) == 0.5f);
  CHECK(GetHalfPixelOffset(3, false) == 0.5f);
}

TEST_CASE("Half-pixel offset in host pixels scales with the axis", "[graphics][half_pixel]") {
  CHECK(GetHalfPixelOffset(1, true) == 0.5f);
  CHECK(GetHalfPixelOffset(2, true) == 0.25f);
  CHECK(GetHalfPixelOffset(4, true) == 0.125f);
  // Half a host pixel at 3x: 1.5 host pixels from the region's corner become 0.5.
  CHECK(GetHalfPixelOffset(3, true) * 3.0f == 0.5f);
}
