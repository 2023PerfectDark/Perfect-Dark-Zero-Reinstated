#include "player_blink.h"
#include "generated/default/pdzstocktu3_init.h"
#include "game_cvars.h"
#include "mousehook.h"
#include <rex/hook.h>
#include <atomic>
#include <bit>
#include <cmath>

namespace pdz_blink {
namespace { double elapsed = 0.0; std::atomic<float> angle{0.f}; }
void Tick(uint8_t* base) {
  if (!pdz_input::Supported() || !pdz_input::WeatherVisible()) return;
  const float dt = std::bit_cast<float>(REX_LOAD_U32(0x82D6C050));
  if (!std::isfinite(dt) || dt <= 0.f || dt > 1.f) return;
  elapsed = std::fmod(elapsed + double(dt), 4.5);
  angle.store(AngleAt(elapsed), std::memory_order_relaxed);
}
float PlayerAngle(uint8_t* base, uint32_t actor) {
  // Native cover/traversal cameras also show Joanna, even with our camera
  // toggle off. The local-player eyelid callback is the scope, not that toggle.
  if (!pdz_input::Supported() || !pdz_input::WeatherVisible()) return -1.f;
  const uint32_t local = REX_LOAD_U32(0x82E36228);
  if (local < 0x10000 || local > 0x7FFFF000 || !actor ||
      REX_LOAD_U32(local + 964) != actor) return -1.f;
  return angle.load(std::memory_order_relaxed);
}
}
