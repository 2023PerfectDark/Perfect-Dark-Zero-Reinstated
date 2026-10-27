#pragma once
/**
 * @file        rex/graphics/util/display_transfer.h
 * @brief       Display output transfer (display_output_transfer = bt1886):
 *              the guest's gamma ramps re-encoded to show the picture as an
 *              HDTV does on an sRGB PC display
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from rexglue-sdk PR #524 (cod3 0040).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <rex/graphics/registers.h>

namespace rex::graphics::display_transfer {

// BT.1886 (Lw 1, Lb 0: L = V^2.4) decoded, then sRGB encoded. Above the sRGB
// toe, sRGB's 1.055 * L^(1/2.4) - 0.055 with L = V^2.4 is 1.055 * V - 0.055.
inline double Bt1886ToSrgb(double v) {
  v = std::clamp(v, 0.0, 1.0);
  const double linear = std::pow(v, 2.4);
  return linear <= 0.0031308 ? 12.92 * linear : 1.055 * v - 0.055;
}

// A 10-bit 256-entry table component.
inline uint32_t Bt1886ToSrgb10(uint32_t value_10b) {
  return uint32_t(std::lround(Bt1886ToSrgb(value_10b / 1023.0) * 1023.0));
}

// A PWL segment: it maps its front buffer steps linearly from base to base +
// delta (the presenter: (base + step * delta / 8) / 65472). The converted
// segment runs from the converted start to the converted end, so the curve
// stays continuous.
inline reg::DC_LUT_PWL_DATA Bt1886ToSrgbPwl(reg::DC_LUT_PWL_DATA raw) {
  constexpr double kPwlScale = 65472.0;
  auto convert = [](double value) { return Bt1886ToSrgb(value / kPwlScale) * kPwlScale; };
  const double start = convert(double(raw.base));
  const double end = convert(double(raw.base) + double(raw.delta));
  reg::DC_LUT_PWL_DATA entry = {};
  entry.base = uint32_t(std::clamp(std::lround(start), 0l, 0xFFFFl));
  entry.delta = uint32_t(std::clamp(std::lround(end - start), 0l, 0xFFFFl));
  return entry;
}

}  // namespace rex::graphics::display_transfer
