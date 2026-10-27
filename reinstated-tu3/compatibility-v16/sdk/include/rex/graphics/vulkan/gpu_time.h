#pragma once
/**
 * @file        rex/graphics/vulkan/gpu_time.h
 * @brief       Graphics card time by kind of work, from timestamps written in
 *              each Vulkan submission (vulkan_gpu_time)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from rexglue-sdk PR #524 (cod3 0055,
 *              0058).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rex::graphics::vulkan {

// What the graphics card is doing. A timestamp is written where the kind
// changes after commands of the old kind were recorded.
enum class GpuSection : uint32_t {
  kDraws,
  kTransfers,
  kResolves,
  kTextureLoads,
  kUploads,
  kSwap,
  kOther,
  kCount,
};

// One timestamp written inside a submission: the time up to it belongs to the
// section it ended.
struct GpuSectionMark {
  uint64_t stamp;
  GpuSection ended;
};

// Adds the ticks of one submission to ticks[section]: from its start stamp to
// each mark, then from the last mark to its end stamp (which ends `last`).
// valid_mask: the timestamp bits the queue writes (a counter that wraps).
inline void AddGpuSectionTicks(uint64_t start, const std::vector<GpuSectionMark>& marks,
                               uint64_t end, GpuSection last, uint64_t valid_mask,
                               uint64_t (&ticks)[size_t(GpuSection::kCount)]) {
  uint64_t previous = start & valid_mask;
  for (const GpuSectionMark& mark : marks) {
    const uint64_t now = mark.stamp & valid_mask;
    ticks[size_t(mark.ended)] += (now - previous) & valid_mask;
    previous = now;
  }
  ticks[size_t(last)] += ((end & valid_mask) - previous) & valid_mask;
}

}  // namespace rex::graphics::vulkan
