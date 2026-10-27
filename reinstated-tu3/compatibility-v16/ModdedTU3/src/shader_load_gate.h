#pragma once
#include <cstdint>
namespace pdz_shaders {
// Nonblocking: the native loading loop continues pumping and drawing while
// workers drain. A short quiet interval catches work arriving from the GPU.
struct LoadGate {
  uint64_t last_started = 0, quiet_since = 0;
  bool active = false;
  void Reset() { *this = {}; }
  bool Hold(uint64_t started, uint64_t finished, uint64_t now) {
    if (!active || started != last_started || started > finished) {
      quiet_since = now;
      last_started = started;
      active = true;
    }
    return started > finished || now - quiet_since < 250;
  }
};
}
