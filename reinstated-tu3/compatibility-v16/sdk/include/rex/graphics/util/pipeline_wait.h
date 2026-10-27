#pragma once
/**
 * @file        rex/graphics/util/pipeline_wait.h
 * @brief       Bounded wait for a pipeline still being created in the
 *              background (async_pipeline_wait_ms), shared by the Vulkan and
 *              Direct3D 12 command processors
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from DPRecomp (pso_missing_policy)
 *              and ForzaHorizon1-ReImagined SDK patch 0051.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <chrono>
#include <cstdint>
#include <thread>

namespace rex::graphics {

// Polls is_ready (a pipeline appeared, or its creation ended) every 100
// microseconds until it returns true or budget_ms have passed. Returns its last
// result; with a budget of 0 or less it is checked only once. A draw whose
// pipeline is still missing afterwards is skipped as before.
template <typename IsReady>
bool WaitForPipelineCreation(IsReady&& is_ready, int32_t budget_ms) {
  if (is_ready()) {
    return true;
  }
  if (budget_ms <= 0) {
    return false;
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
  do {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    if (is_ready()) {
      return true;
    }
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

}  // namespace rex::graphics
