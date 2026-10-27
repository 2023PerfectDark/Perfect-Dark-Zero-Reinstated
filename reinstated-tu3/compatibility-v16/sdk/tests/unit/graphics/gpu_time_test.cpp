/**
 * @file        graphics/gpu_time_test.cpp
 * @brief       Tests for the graphics card time by kind from submission
 *              timestamps (vulkan_gpu_time)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/vulkan/gpu_time.h>

using rex::graphics::vulkan::AddGpuSectionTicks;
using rex::graphics::vulkan::GpuSection;
using rex::graphics::vulkan::GpuSectionMark;

namespace {

uint64_t Ticks(const uint64_t (&ticks)[size_t(GpuSection::kCount)], GpuSection section) {
  return ticks[size_t(section)];
}

}  // namespace

TEST_CASE("A submission without marks counts for its last section", "[graphics][gpu_time]") {
  uint64_t ticks[size_t(GpuSection::kCount)] = {};
  AddGpuSectionTicks(100, {}, 160, GpuSection::kDraws, ~uint64_t(0), ticks);
  CHECK(Ticks(ticks, GpuSection::kDraws) == 60);
  CHECK(Ticks(ticks, GpuSection::kOther) == 0);
}

TEST_CASE("Each mark ends the section before it", "[graphics][gpu_time]") {
  uint64_t ticks[size_t(GpuSection::kCount)] = {};
  const std::vector<GpuSectionMark> marks = {
      {110, GpuSection::kTextureLoads}, {150, GpuSection::kDraws}, {155, GpuSection::kResolves}};
  AddGpuSectionTicks(100, marks, 200, GpuSection::kDraws, ~uint64_t(0), ticks);
  CHECK(Ticks(ticks, GpuSection::kTextureLoads) == 10);
  CHECK(Ticks(ticks, GpuSection::kDraws) == 40 + 45);
  CHECK(Ticks(ticks, GpuSection::kResolves) == 5);
  // The kinds add up to the submission.
  uint64_t total = 0;
  for (uint64_t t : ticks) {
    total += t;
  }
  CHECK(total == 100);
}

TEST_CASE("Timestamps that wrap within their valid bits are still counted",
          "[graphics][gpu_time]") {
  // A queue with 32 valid bits: the counter wraps from 0xFFFFFFF0 to 0x10.
  const uint64_t mask = 0xFFFFFFFFull;
  uint64_t ticks[size_t(GpuSection::kCount)] = {};
  AddGpuSectionTicks(0xFFFFFFF0ull, {{0x00000008ull, GpuSection::kUploads}}, 0x00000010ull,
                     GpuSection::kSwap, mask, ticks);
  CHECK(Ticks(ticks, GpuSection::kUploads) == 0x18);
  CHECK(Ticks(ticks, GpuSection::kSwap) == 0x8);
}
