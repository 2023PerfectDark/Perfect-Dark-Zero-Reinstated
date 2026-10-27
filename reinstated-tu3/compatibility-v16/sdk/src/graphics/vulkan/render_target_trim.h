#pragma once
/**
 * @file        graphics/vulkan/render_target_trim.h
 * @brief       When to free the cached render targets that hold no EDRAM data
 *              (vulkan_render_target_trim_percent)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ForzaHorizon1-ReImagined SDK
 *              patch 0112 (MIT, Copyright (c) 2026 Dyotson and Forza Horizon 1:
 *              ReImagined contributors).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstdint>

namespace rex::graphics::vulkan::render_target_trim {

// The render target cache keeps one image per EDRAM layout for the whole
// session; at resolution scale 3 each can be about 90 MB, and after hours of
// play device memory runs out (render targets and resolves fail).

// Frames between budget queries, and before the next trim (a trim waits for
// the GPU).
constexpr uint64_t kCheckInterval = 30;
constexpr uint64_t kAfterFailure = 60;
constexpr uint64_t kAfterTrim = 300;
constexpr uint64_t kAfterSmallTrim = 1800;
constexpr uint64_t kSmallTrimBytes = uint64_t(256) << 20;

// Called at every frame end. True: query the memory budget now (after a failed
// allocation, trim whatever it says).
inline bool ShouldQuery(uint64_t frame, uint64_t& next_check_frame, uint64_t next_allowed_frame,
                        uint32_t percent, bool allocation_failed, bool have_budget) {
  if (!percent || frame < next_allowed_frame) {
    return false;
  }
  if (allocation_failed) {
    return true;
  }
  if (!have_budget || frame < next_check_frame) {
    return false;
  }
  next_check_frame = frame + kCheckInterval;
  return true;
}

inline bool OverBudget(uint64_t usage, uint64_t budget, uint32_t percent) {
  return percent && budget && usage * 100 >= budget * percent;
}

// The first frame of the next trim. Little freed: the memory is full of other
// things, so wait longer before the next GPU wait.
inline uint64_t NextAllowedFrame(uint64_t frame, bool after_failure, uint64_t freed_bytes) {
  return frame + (freed_bytes < kSmallTrimBytes ? kAfterSmallTrim
                  : after_failure              ? kAfterFailure
                                               : kAfterTrim);
}

}  // namespace rex::graphics::vulkan::render_target_trim
