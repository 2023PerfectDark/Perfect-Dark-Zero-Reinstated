/**
 * @file        graphics/float_constant_runs_test.cpp
 * @brief       Tests for CopyFloatConstantRuns (float constants packed in runs)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include <rex/graphics/util/float_constants.h>

namespace {

// The per-constant loop the command processors used before.
uint8_t* CopyOneByOne(uint8_t* destination, const uint32_t* first_constant,
                      const uint64_t used_bitmap[4]) {
  for (uint32_t i = 0; i < 4; ++i) {
    uint64_t bits = used_bitmap[i];
    while (bits) {
      const uint32_t index = uint32_t(std::countr_zero(bits));
      bits &= bits - 1;
      std::memcpy(destination, first_constant + ((i * 64 + index) << 2), sizeof(uint32_t) * 4);
      destination += sizeof(uint32_t) * 4;
    }
  }
  return destination;
}

void CheckSame(const uint64_t used_bitmap[4]) {
  std::vector<uint32_t> constants(256 * 4);
  for (uint32_t i = 0; i < constants.size(); ++i) {
    constants[i] = 0x9E3779B9u * (i + 1);
  }
  // Room for all 256 constants plus a guard after them.
  std::vector<uint8_t> expected(256 * 16 + 16, 0xCD);
  std::vector<uint8_t> actual(256 * 16 + 16, 0xCD);
  const uint8_t* expected_end = CopyOneByOne(expected.data(), constants.data(), used_bitmap);
  const uint8_t* actual_end =
      rex::graphics::CopyFloatConstantRuns(actual.data(), constants.data(), used_bitmap);
  REQUIRE(actual_end - actual.data() == expected_end - expected.data());
  REQUIRE(actual == expected);
}

}  // namespace

TEST_CASE("CopyFloatConstantRuns packs like the per-constant loop", "[graphics][constants]") {
  SECTION("edge cases") {
    const uint64_t all = ~uint64_t(0);
    const uint64_t cases[][4] = {
        {0, 0, 0, 0},
        {all, all, all, all},
        {1, 0, 0, uint64_t(1) << 63},
        {uint64_t(1) << 63, 1, 0, 0},
        {0x5555555555555555ull, 0xAAAAAAAAAAAAAAAAull, 0, all},
        {0xFFFF0000FFFF0000ull, 0x00000000FFFFFFFFull, 0xF0F0F0F0F0F0F0F0ull, 0x8000000000000001ull},
        {all >> 1, all << 1, 0x7FFFFFFFFFFFFFFEull, 0},
    };
    for (const auto& bitmap : cases) {
      CheckSame(bitmap);
    }
  }

  SECTION("random masks") {
    std::mt19937_64 random(0xC0D3);
    for (int n = 0; n < 2000; ++n) {
      uint64_t bitmap[4];
      for (uint64_t& word : bitmap) {
        // Mix sparse, dense and run-heavy words.
        switch (random() % 3) {
          case 0:
            word = random() & random();
            break;
          case 1:
            word = random() | random();
            break;
          default: {
            const uint32_t start = uint32_t(random() % 64);
            const uint32_t length = uint32_t(random() % 65);
            word = length >= 64 ? ~uint64_t(0) : ((uint64_t(1) << length) - 1);
            word <<= start;
            break;
          }
        }
      }
      CheckSame(bitmap);
    }
  }
}
