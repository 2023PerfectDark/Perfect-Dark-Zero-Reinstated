/**
 * @file        graphics/render_target_trim_test.cpp
 * @brief       Tests for when the Vulkan render target cache is trimmed
 *              (vulkan_render_target_trim_percent)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <graphics/vulkan/render_target_trim.h>

using namespace rex::graphics::vulkan::render_target_trim;

TEST_CASE("Trimming off never queries the budget", "[graphics][render_target_trim]") {
  uint64_t next_check = 0;
  CHECK_FALSE(ShouldQuery(100, next_check, 0, 0, true, true));
  CHECK_FALSE(OverBudget(100, 100, 0));
}

TEST_CASE("The budget is queried every few frames", "[graphics][render_target_trim]") {
  uint64_t next_check = 0;
  CHECK(ShouldQuery(10, next_check, 0, 90, false, true));
  CHECK(next_check == 10 + kCheckInterval);
  CHECK_FALSE(ShouldQuery(11, next_check, 0, 90, false, true));
  CHECK(ShouldQuery(10 + kCheckInterval, next_check, 0, 90, false, true));
  // Without VK_EXT_memory_budget only a failed allocation trims.
  uint64_t no_budget_check = 0;
  CHECK_FALSE(ShouldQuery(10, no_budget_check, 0, 90, false, false));
  CHECK(ShouldQuery(10, no_budget_check, 0, 90, true, false));
}

TEST_CASE("A trim waits before the next one", "[graphics][render_target_trim]") {
  uint64_t next_check = 0;
  CHECK_FALSE(ShouldQuery(50, next_check, 100, 90, true, true));
  CHECK(NextAllowedFrame(1000, false, kSmallTrimBytes) == 1000 + kAfterTrim);
  CHECK(NextAllowedFrame(1000, true, kSmallTrimBytes) == 1000 + kAfterFailure);
  // Little freed: the memory is full of other things.
  CHECK(NextAllowedFrame(1000, true, kSmallTrimBytes - 1) == 1000 + kAfterSmallTrim);
}

TEST_CASE("The budget threshold is a percentage of device-local memory",
          "[graphics][render_target_trim]") {
  CHECK(OverBudget(90, 100, 90));
  CHECK_FALSE(OverBudget(89, 100, 90));
  CHECK_FALSE(OverBudget(90, 0, 90));
  // No overflow with real sizes.
  CHECK(OverBudget(uint64_t(15) << 30, uint64_t(16) << 30, 90));
}
