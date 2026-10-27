#include "generated/default/perfectdarkzerorecomp_init.h"
#include "game_cvars.h"
#include "mousehook.h"
#include "physics_timing.h"

#include <atomic>
#include <bit>
#include <cstring>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_BOOL(pdz_lab_key_stability, true, "Gameplay",
    "Keep Mission 5's placed desk keycard at its authored position");

namespace {
bool Readable(uint32_t address, uint32_t size) {
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !address || uint64_t(address) + size > 0x100000000ull) return false;
  for (uint64_t at = address; at < uint64_t(address) + size;) {
    auto* heap = runtime->memory()->LookupHeap(uint32_t(at));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(at), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    at = (at / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
bool IsLabDeskKey(uint8_t* base, uint32_t prop) {
  if (!Readable(prop, 96) || REX_LOAD_U32(prop + 12) != 14) return false;
  const auto session = REX_LOAD_U32(0x82D6B2B0);
  const auto setup = REX_LOAD_U32(prop + 84);
  if (!Readable(session, 40) || !Readable(setup, 144)) return false;
  const auto name = REX_LOAD_U32(setup + 140);
  constexpr char model[] = "aid_darkcharacter_props_niknak_keycard";
  if (!Readable(name, sizeof(model)) || std::memcmp(base + name, model, sizeof(model))) return false;
  std::array<float, 3> position{};
  for (unsigned i = 0; i < position.size(); ++i)
    position[i] = std::bit_cast<float>(REX_LOAD_U32(setup + 4 + i * 4));
  return pdz_physics::IsLabDeskKey(REX_LOAD_U8(session + 25),
      REX_LOAD_U32(session + 36), REX_LOAD_U32(prop + 12),
      REX_LOAD_U32(prop + 92), model, position);
}
}

REX_EXTERN(__imp__sub_825D14D0);
REX_HOOK_RAW(sub_825D14D0) {
  const auto descriptor = ctx.r5.u32;
  // 82522190's placed animated-prop constructor provides this stack descriptor.
  // In TU0 Havok, motion 6 is keyframed (825D50C8), 7 is fixed (829D4C60).
  // Keep native body registration, rendering, glow and proximity pickup intact;
  // only this desk key's solver response becomes fixed. Do not change tables.
  if (pdz_input::Supported() && REXCVAR_GET(pdz_lab_key_stability) &&
      Readable(descriptor, 36) && REX_LOAD_U32(descriptor + 32) == 1 &&
      IsLabDeskKey(base, ctx.r4.u32)) {
    struct Restore {
      uint8_t* base;
      uint32_t address, value;
      ~Restore() { REX_STORE_U32(address, value); }
    } restore{base, descriptor + 32, REX_LOAD_U32(descriptor + 32)};
    REX_STORE_U32(descriptor + 32, 7);
    __imp__sub_825D14D0(ctx, base);
    REXLOG_INFO("Mission 5 desk keycard: fixed native pickup body created");
    return;
  }
  __imp__sub_825D14D0(ctx, base);
}

REX_EXTERN(__imp__sub_829B61D0);
REX_HOOK_RAW(sub_829B61D0) {
  // TU0 hkpWorld step entry, used by both the main and Havok worker paths.
  // f1 = elapsed game time, f2 = solver step. The original virtual simulation
  // handles integration, collision, interpolation and return/error status.
  // Do not change f1, the global game clock, or the number of rendered frames.
  if (pdz_input::Supported() && REXCVAR_GET(pdz_physics_timing_fix)) {
    const float frame = float(ctx.f1.f64);
    const float requested = float(ctx.f2.f64);
    const float step = pdz_physics::SolverStep(frame, requested);
    if (step < requested) {
      ctx.f2.f64 = step;
      static std::atomic<bool> reported{false};
      if (!reported.exchange(true))
        REXLOG_INFO("PDZ physics timing: frame {:.6f}s, solver {:.6f}s -> {:.6f}s (caller {:08X})",
                    frame, requested, step, uint32_t(ctx.lr));
    }
  }
  __imp__sub_829B61D0(ctx, base);
}
