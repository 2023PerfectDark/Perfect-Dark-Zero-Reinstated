/**
 * @file        ppc/fp_fma_test.cpp
 * @brief       Tests for the scalar multiply-add of recompiled fmadd and its kin
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include <rex/ppc/intrinsics.h>

using rex::ppc::fp_fma;
using rex::ppc::fp_mul_add;

namespace {

// (1 + 2^-30) * (1 - 2^-30) = 1 - 2^-60, which rounds to 1 as a double.
constexpr double kA = 1.0 + 0x1p-30;
constexpr double kB = 1.0 - 0x1p-30;

}  // namespace

TEST_CASE("The multiply-add rounds the product first", "[ppc][fma]") {
  CHECK(fp_mul_add(kA, kB, -1.0) == 0.0);
  CHECK(fp_mul_add(2.0, 3.0, 4.0) == 10.0);
}

TEST_CASE("The fused multiply-add rounds once", "[ppc][fma]") {
  CHECK(std::fma(kA, kB, -1.0) == -0x1p-60);
}

TEST_CASE("fp_fma is fused on a CPU with FMA, a multiply-add without", "[ppc][fma]") {
  const double r = fp_fma(kA, kB, -1.0);
  // Either way, exact results stay exact.
  CHECK(fp_fma(2.0, 3.0, 4.0) == 10.0);
#if defined(__FMA__) || defined(__aarch64__) || defined(_M_ARM64)
  CHECK(r == -0x1p-60);
#else
  CHECK(r == (rex::ppc::fp_host_has_fma ? -0x1p-60 : 0.0));
#endif
}
