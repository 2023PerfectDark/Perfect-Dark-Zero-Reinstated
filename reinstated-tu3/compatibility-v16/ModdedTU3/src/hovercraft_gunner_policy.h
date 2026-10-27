#pragma once
#include <cmath>

namespace pdz_gunner {
inline bool Enabled(int style, unsigned mission, unsigned mode,
                    unsigned body, unsigned weapon) {
  return style == 1 && mission == 6 && mode == 0 && body == 7 &&
         (weapon == 10 || weapon == 23);
}
inline float AimDelta(float dt) {
  // Only the aim interpolation sees this delta, never the simulation clock.
  return std::isfinite(dt) && dt > 0.f ? dt * 4.f : dt;
}
}
