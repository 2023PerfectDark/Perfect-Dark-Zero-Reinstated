#pragma once
#include <cmath>
#include <cstdint>

namespace pdz_input {
// A bounded virtual thumbstick. Retain its position while stationary so the
// player can hold a pin's sweet spot; no automatic rotation or completion.
struct LockpickMouse {
  uint32_t interaction{};
  float x{}, y{};
  void Reset() { interaction=0; x=y=0; }
  void Move(uint32_t id, float dx, float dy) {
    if (!id) { Reset(); return; }
    if (id!=interaction) { Reset(); interaction=id; }
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    x+=dx/120.f; y-=dy/120.f;
    const float radius=std::hypot(x,y);
    if(radius>1.f) { x/=radius; y/=radius; }
  }
};
}
