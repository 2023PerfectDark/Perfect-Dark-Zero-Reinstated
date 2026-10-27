#pragma once
/**
 * @file        rex/graphics/util/float_constants.h
 * @brief       Packing of the float constants a shader uses into its constant
 *              buffer, shared by the Vulkan and Direct3D 12 command processors
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from rexglue-sdk PR #524 (cod3 0057).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <bit>
#include <cstdint>
#include <cstring>

namespace rex::graphics {

// Copies the float constants marked in used_bitmap (256 bits, one per float4
// constant) from first_constant (the registers of constant 0, 4 dwords per
// constant) to destination, packed in register order, and returns the end of
// what was written. Each run of consecutive constants is one copy instead of
// one copy per constant: shaders mostly use long runs, and the destination is
// write-combined upload memory, where wide sequential stores are fast.
inline uint8_t* CopyFloatConstantRuns(uint8_t* destination, const uint32_t* first_constant,
                                      const uint64_t used_bitmap[4]) {
  for (uint32_t i = 0; i < 4; ++i) {
    uint64_t bits = used_bitmap[i];
    while (bits) {
      const uint32_t start = uint32_t(std::countr_zero(bits));
      const uint32_t length = uint32_t(std::countr_one(bits >> start));
      const size_t size = sizeof(uint32_t) * 4 * length;
      std::memcpy(destination, first_constant + ((i * 64 + start) << 2), size);
      destination += size;
      bits = start + length >= 64 ? 0 : bits & (~uint64_t(0) << (start + length));
    }
  }
  return destination;
}

}  // namespace rex::graphics
