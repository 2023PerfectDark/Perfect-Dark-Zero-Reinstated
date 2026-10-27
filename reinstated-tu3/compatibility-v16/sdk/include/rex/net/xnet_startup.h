#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    2026 - Ported for the ReXGlue fork
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rex::net {

// XNetStartupParams as the guest lays them out.
inline constexpr size_t kXNetStartupParamsSize = 13;
using XNetStartupParams = std::array<uint8_t, kXNetStartupParamsSize>;

// What XNet runs with for the parameters a title passed, none when empty. Zero
// fields take xam's defaults and the others are clamped to its limits.
inline XNetStartupParams NormalizeXNetStartupParams(std::span<const uint8_t> params) {
  constexpr XNetStartupParams kDefaults{0, 0, 8, 32, 16, 16, 8, 32, 64, 2, 3, 8, 2};
  constexpr XNetStartupParams kMinimums{0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  constexpr XNetStartupParams kMaximums{0,   255, 232, 232, 63,  63, 255,
                                        255, 255, 255, 255, 255, 255};
  XNetStartupParams result = kDefaults;
  // The extended struct starts with the same fields, any other size is
  // ignored.
  if (params.size() >= kXNetStartupParamsSize &&
      (params[0] == kXNetStartupParamsSize || params[0] == 0x4C)) {
    for (size_t i = 1; i < kXNetStartupParamsSize; ++i) {
      if (params[i]) {
        result[i] = std::clamp(params[i], kMinimums[i], kMaximums[i]);
      }
    }
  }
  // Retail xam clears the flags that turn security off.
  result[1] &= 0xF6;
  result[0] = kXNetStartupParamsSize;
  return result;
}

}  // namespace rex::net
