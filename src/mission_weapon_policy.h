#pragma once
#include <array>
#include <cstdint>

namespace pdz_mission_weapons {
// Unused in the supported retail weapon registry; outside native unlock IDs.
inline constexpr uint32_t kKlobb = 39;
constexpr bool MissionOnly(uint32_t id) { return id == kKlobb; }
using Loadout = std::array<uint32_t, 4>;
constexpr Loadout PersistentLoadout(const Loadout& live) {
  bool temporary = false;
  for (auto id : live) temporary |= MissionOnly(id);
  if (!temporary) return live;
  Loadout saved{};
  unsigned next = 0;
  for (auto id : live)
    if (id && !MissionOnly(id)) saved[next++] = id;
  return saved;
}
}
