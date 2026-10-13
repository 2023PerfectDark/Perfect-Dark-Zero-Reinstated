#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace pdz_holsters {
struct Weapon {
  uint32_t object{}, model{}, resource{};
  bool two_handed{}, equipped{};
};
using Layout = std::array<Weapon, 4>; // Back left/right, thigh right/left.
inline Weapon SelectIntroWeapon(std::span<const Weapon> inventory) {
  Weapon result{};
  for (const auto& weapon : inventory) {
    if (!weapon.object || !weapon.model || !weapon.resource || weapon.two_handed)
      continue;
    if (weapon.equipped) return weapon;
    if (!result.object) result=weapon;
  }
  return result; // Caller supplies a cosmetic P9P when no one-handed gun exists.
}
inline Layout Select(std::span<const Weapon> inventory) {
  Layout result{};
  size_t backs = 0, thighs = 0;
  for (const auto& weapon : inventory) {
    if (!weapon.object || !weapon.model || !weapon.resource || weapon.equipped)
      continue;
    bool duplicate = false;
    for (const auto& slot : result) duplicate |= slot.object == weapon.object;
    if (duplicate) continue;
    if (weapon.two_handed && backs < 2) result[backs++] = weapon;
    if (!weapon.two_handed && thighs < 2) result[2 + thighs++] = weapon;
  }
  return result;
}
} // namespace pdz_holsters
