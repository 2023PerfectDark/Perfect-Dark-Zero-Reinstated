#pragma once
#include <algorithm>
#include <cstdint>
namespace pdz_backpack {
struct Ammo { uint16_t magazine, reserve; };
constexpr Ammo Reload(uint16_t magazine, uint16_t capacity, uint16_t reserve) {
  if (!capacity || capacity == UINT16_MAX || magazine >= capacity) return {magazine,reserve};
  const auto moved = std::min<unsigned>(capacity-magazine,reserve);
  return {uint16_t(magazine+moved),reserve==UINT16_MAX?reserve:uint16_t(reserve-moved)};
}
}
