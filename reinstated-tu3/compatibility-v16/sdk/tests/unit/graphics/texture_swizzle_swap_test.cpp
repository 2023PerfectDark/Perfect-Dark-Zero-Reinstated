/**
 * @file        graphics/texture_swizzle_swap_test.cpp
 * @brief       Tests for swapping red and blue sources in a host swizzle
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/pipeline/texture/cache.h>
#include <rex/graphics/xenos.h>

using rex::graphics::TextureCache;

TEST_CASE("Red and blue sources swap in a host swizzle", "[graphics][texture_cache]") {
  CHECK(TextureCache::SwapRedAndBlueSources(XE_GPU_MAKE_TEXTURE_SWIZZLE(R, G, B, A)) ==
        uint32_t(XE_GPU_MAKE_TEXTURE_SWIZZLE(B, G, R, A)));
  // Constants and alpha stay; red read twice becomes blue read twice.
  CHECK(TextureCache::SwapRedAndBlueSources(XE_GPU_MAKE_TEXTURE_SWIZZLE(R, R, 0, 1)) ==
        uint32_t(XE_GPU_MAKE_TEXTURE_SWIZZLE(B, B, 0, 1)));
}

TEST_CASE("Swapping red and blue twice gives the swizzle back", "[graphics][texture_cache]") {
  for (uint32_t swizzle = 0; swizzle < (UINT32_C(1) << 12); ++swizzle) {
    CHECK(TextureCache::SwapRedAndBlueSources(TextureCache::SwapRedAndBlueSources(swizzle)) ==
          swizzle);
  }
}
