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

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <rex/net/xnet_startup.h>

using rex::net::NormalizeXNetStartupParams;
using rex::net::XNetStartupParams;

TEST_CASE("XNet startup parameters take xam's defaults and limits", "[kernel][xnet]") {
  const XNetStartupParams defaults{13, 0, 8, 32, 16, 16, 8, 32, 64, 2, 3, 8, 2};
  // Lost Planet 2 sets the size and leaves the rest to XNet.
  const uint8_t unset[13] = {13};
  CHECK(NormalizeXNetStartupParams(unset) == defaults);
  CHECK(NormalizeXNetStartupParams({}) == defaults);
  const uint8_t unknown_size[13] = {12, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
  CHECK(NormalizeXNetStartupParams(unknown_size) == defaults);
  const uint8_t given[13] = {13, 0x0F, 255, 1, 100, 63, 255, 1, 16, 9, 9, 9, 9};
  CHECK(NormalizeXNetStartupParams(given) ==
        XNetStartupParams{13, 0x06, 232, 1, 63, 63, 255, 1, 16, 9, 9, 9, 9});
  std::array<uint8_t, 0x4C> extended{0x4C};
  extended[8] = 16;
  auto from_extended = defaults;
  from_extended[8] = 16;
  CHECK(NormalizeXNetStartupParams(extended) == from_extended);
}
