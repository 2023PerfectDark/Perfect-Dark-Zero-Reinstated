#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace pdz_holsters {
struct Weapon {
  uint32_t object{}, model{}, resource{};
  bool two_handed{}, equipped{};
  uint32_t id{};
};
using Layout = std::array<Weapon, 5>; // Back left/right, thigh right/left, centre-back shield.
// Return an existing live copy or an unused cache entry. Never evict an old
// graph during weapon switching; the native owner controls its lifetime.
template <typename Slots, typename Live>
inline size_t ResidentSlot(const Slots& slots, uint32_t owner,
                           const Weapon& desired, Live live) {
  for (size_t i=0; i<slots.size(); ++i)
    if (slots[i].owner==owner && slots[i].weapon==desired.object &&
        slots[i].model==desired.model && live(slots[i])) return i;
  for (size_t i=0; i<slots.size(); ++i)
    if (!slots[i].object) return i;
  return slots.size();
}
inline Weapon SelectIntroWeapon(std::span<const Weapon> inventory) {
  Weapon result{};
  for (const auto& weapon : inventory) {
    if (!weapon.object || !weapon.model || !weapon.resource || weapon.two_handed || weapon.id == 26)
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
    if (weapon.id == 26) { if (!result[4].object) result[4]=weapon; continue; }
    if (weapon.two_handed && backs < 2) result[backs++] = weapon;
    if (!weapon.two_handed && thighs < 2) result[2 + thighs++] = weapon;
  }
  return result;
}
// Retain a stored weapon's slot across equip/unequip transitions. Compacting
// inventory order needlessly destroyed and recreated the other attachments.
inline Layout SelectStable(std::span<const Weapon> inventory, const Layout& previous) {
  Layout result{};
  auto eligible = [](const Weapon& w) {
    return w.object && w.model && w.resource && !w.equipped;
  };
  auto fits = [](const Weapon& w, size_t slot) {
    return w.id == 26 ? slot == 4 : slot < 4 && (w.two_handed == (slot < 2));
  };
  auto present = [&](uint32_t object) {
    for (const auto& w : result) if (w.object == object) return true;
    return false;
  };
  for (size_t slot = 0; slot < result.size(); ++slot)
    for (const auto& w : inventory)
      if (eligible(w) && fits(w, slot) && w.object == previous[slot].object &&
          w.model == previous[slot].model && !present(w.object)) {
        result[slot] = w; break;
      }
  for (const auto& w : inventory) {
    if (!eligible(w) || present(w.object)) continue;
    for (size_t slot = 0; slot < result.size(); ++slot)
      if (!result[slot].object && fits(w, slot)) { result[slot] = w; break; }
  }
  return result;
}
} // namespace pdz_holsters

