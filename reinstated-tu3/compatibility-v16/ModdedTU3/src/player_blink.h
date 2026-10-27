#pragma once
#include <cstdint>
namespace pdz_blink {
// One 200 ms close/open motion every 4.5 seconds, in simulation time.
inline float AngleAt(double seconds) {
  if (seconds < 0.0) return 0.f;
  const double cycle = seconds - static_cast<uint64_t>(seconds / 4.5) * 4.5;
  if (cycle < 4.3) return 0.f;
  const double t = (cycle - 4.3) / .2;
  const double triangular = t < .5 ? t * 2.0 : (1.0 - t) * 2.0;
  return float(.75 * triangular * triangular * (3.0 - 2.0 * triangular));
}
void Tick(uint8_t* base);
// Negative means retain the original actor's animation.
float PlayerAngle(uint8_t* base, uint32_t actor);
}
