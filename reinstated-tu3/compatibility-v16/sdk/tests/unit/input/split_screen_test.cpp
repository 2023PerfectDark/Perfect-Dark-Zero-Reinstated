/**
 * @file        input/split_screen_test.cpp
 * @brief       Tests for the split-screen device choice (input_player1/2,
 *              keyboard_own_player)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ArmyOfTwoRecompilation
 *              (sdk_changes, device_assignment.cpp).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rex/cvar.h>
#include <rex/input/device_assignment.h>

REXCVAR_DECLARE(std::string, input_player1);
REXCVAR_DECLARE(std::string, input_player2);
REXCVAR_DECLARE(bool, keyboard_own_player);

using rex::input::DeviceId;
using rex::input::DeviceInfo;
using rex::input::KeyboardUser;
using rex::input::kNoUser;
using rex::input::ParsePlayerDevices;
using rex::input::SlotAssignment;

namespace {

DeviceInfo Pad(uint64_t id, uint32_t ordinal) {
  DeviceInfo info;
  info.id = static_cast<DeviceId>(id);
  info.ordinal = ordinal;
  return info;
}

DeviceInfo Keyboard(uint64_t id) {
  DeviceInfo info;
  info.id = static_cast<DeviceId>(id);
  info.synthetic = true;
  info.name = "Keyboard and Mouse";
  return info;
}

std::vector<DeviceId> For(const SlotAssignment& a, uint32_t user) {
  std::vector<DeviceId> out;
  a.DevicesForUser(user, out);
  return out;
}

std::vector<DeviceId> Ids(std::initializer_list<uint64_t> ids) {
  std::vector<DeviceId> out;
  for (uint64_t id : ids) {
    out.push_back(static_cast<DeviceId>(id));
  }
  return out;
}

// Restores the split-screen cvars after a test.
struct SplitScreenCvars {
  ~SplitScreenCvars() {
    REXCVAR_SET(input_player1, "auto");
    REXCVAR_SET(input_player2, "auto");
    REXCVAR_SET(keyboard_own_player, false);
  }
};

}  // namespace

TEST_CASE("Player device lists are parsed", "[input][split_screen]") {
  auto d = ParsePlayerDevices("keyboard, pad2", false);
  CHECK(d.keyboard);
  CHECK_FALSE(d.pads);
  CHECK(d.pad_mask == 0b10);
  d = ParsePlayerDevices("pads", false);
  CHECK(d.pads);
  CHECK_FALSE(d.keyboard);
  // auto: player 1 keeps what nobody else names, player 2 gets nothing.
  CHECK(ParsePlayerDevices("auto", true).pads);
  CHECK(ParsePlayerDevices("auto", true).keyboard);
  CHECK_FALSE(ParsePlayerDevices("auto", false).pads);
  CHECK(ParsePlayerDevices("none", true).pad_mask == 0);
  CHECK(ParsePlayerDevices("pad5", true).pad_mask == 0);
}

TEST_CASE("By default pad N is player N and the keyboard player 1", "[input][split_screen]") {
  SplitScreenCvars restore;
  SlotAssignment slots;
  slots.OnDevicesChanged({Keyboard(1), Pad(10, 0), Pad(11, 1)});
  CHECK(For(slots, 0) == Ids({1, 10}));
  CHECK(For(slots, 1) == Ids({11}));
  CHECK(KeyboardUser() == 0);
}

TEST_CASE("keyboard_own_player moves the pads one player on", "[input][split_screen]") {
  SplitScreenCvars restore;
  REXCVAR_SET(keyboard_own_player, true);
  SlotAssignment slots;
  slots.OnDevicesChanged({Keyboard(1), Pad(10, 0)});
  CHECK(For(slots, 0) == Ids({1}));
  CHECK(For(slots, 1) == Ids({10}));
}

TEST_CASE("Chosen devices go to their players", "[input][split_screen]") {
  SplitScreenCvars restore;
  // Player 1 on the second pad, player 2 on the keyboard and the first pad.
  REXCVAR_SET(input_player1, "pad2");
  REXCVAR_SET(input_player2, "keyboard,pad1");
  SlotAssignment slots;
  slots.OnDevicesChanged({Keyboard(1), Pad(10, 0), Pad(11, 1), Pad(12, 2)});
  CHECK(For(slots, 0) == Ids({11}));
  CHECK(For(slots, 1) == Ids({1, 10}));
  CHECK(KeyboardUser() == 1);

  // A pad both name is the first player's; pads nobody names stay unused.
  REXCVAR_SET(input_player1, "pad1");
  REXCVAR_SET(input_player2, "pad1,pad2");
  slots.OnDevicesChanged({Keyboard(1), Pad(10, 0), Pad(11, 1), Pad(12, 2)});
  CHECK(For(slots, 0) == Ids({10}));
  CHECK(For(slots, 1) == Ids({11}));
  CHECK(KeyboardUser() == kNoUser);
}
