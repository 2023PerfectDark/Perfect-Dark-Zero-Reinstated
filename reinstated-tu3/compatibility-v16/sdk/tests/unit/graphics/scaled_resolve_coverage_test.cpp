/**
 * @file        graphics/scaled_resolve_coverage_test.cpp
 * @brief       Tests for the scaled resolve coverage threshold of textures
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/pipeline/texture/cache.h>

using rex::graphics::TextureCache;

TEST_CASE("Any resolved page makes a scaled resolve texture at 0 percent",
          "[graphics][texture_cache]") {
  CHECK(TextureCache::IsScaledResolveCoverageEnough(1, 1024, 0));
  CHECK_FALSE(TextureCache::IsScaledResolveCoverageEnough(0, 1024, 0));
}

TEST_CASE("A coverage threshold needs that share of the texture's pages",
          "[graphics][texture_cache]") {
  // A resolve overlapping only the last mip pages of a large texture.
  CHECK_FALSE(TextureCache::IsScaledResolveCoverageEnough(3, 342, 50));
  CHECK_FALSE(TextureCache::IsScaledResolveCoverageEnough(170, 342, 50));
  CHECK(TextureCache::IsScaledResolveCoverageEnough(171, 342, 50));
  CHECK(TextureCache::IsScaledResolveCoverageEnough(342, 342, 100));
  CHECK_FALSE(TextureCache::IsScaledResolveCoverageEnough(0, 342, 50));
}
