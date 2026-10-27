/**
 * @file        core/bit_scan_test.cpp
 * @brief       Tests for rex::bit_scan_forward (inline, as in canary)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include <rex/math.h>

TEST_CASE("bit_scan_forward finds the lowest set bit", "[math]") {
  uint32_t index = 123;
  REQUIRE_FALSE(rex::bit_scan_forward(uint32_t(0), &index));
  REQUIRE_FALSE(rex::bit_scan_forward(uint64_t(0), &index));
  for (uint32_t bit = 0; bit < 32; ++bit) {
    const uint32_t value = (UINT32_C(1) << bit) | (bit < 31 ? UINT32_C(0x80000000) : 0);
    REQUIRE(rex::bit_scan_forward(value, &index));
    REQUIRE(index == bit);
    REQUIRE(rex::bit_scan_forward(int32_t(value), &index));
    REQUIRE(index == bit);
  }
  for (uint32_t bit = 0; bit < 64; ++bit) {
    const uint64_t value = (UINT64_C(1) << bit) | UINT64_C(0x8000000000000000);
    REQUIRE(rex::bit_scan_forward(value, &index));
    REQUIRE(index == bit);
    REQUIRE(rex::bit_scan_forward(int64_t(value), &index));
    REQUIRE(index == bit);
  }
}
