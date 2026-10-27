/**
 * @file        ui/keybind_names_test.cpp
 * @brief       Tests for the key names binds accept
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>

using rex::ui::ParseVirtualKey;
using rex::ui::VirtualKey;

TEST_CASE("Mouse buttons are bindable by name", "[ui][keybinds]") {
  CHECK(ParseVirtualKey("LMB") == VirtualKey::kLButton);
  CHECK(ParseVirtualKey("RMB") == VirtualKey::kRButton);
  CHECK(ParseVirtualKey("MMB") == VirtualKey::kMButton);
  // The side buttons.
  CHECK(ParseVirtualKey("MB4") == VirtualKey::kXButton1);
  CHECK(ParseVirtualKey("MB5") == VirtualKey::kXButton2);
  CHECK(ParseVirtualKey("MB6") == VirtualKey::kNone);
}
