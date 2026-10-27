#include "generated/default/perfectdarkzerorecomp_init.h"
#include "mousehook.h"
#include "blood_decals.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <chrono>
#include <mutex>
#include <rex/logging.h>
#include <atomic>

REXCVAR_DEFINE_BOOL(pdz_extra_blood, false, "Gameplay",
    "Experimental: add one native blood burst on character hits (at most eight extra bursts per second)");
REXCVAR_DEFINE_BOOL(pdz_blood_decal_trace, false, "Debug",
    "Record bounded native hit/decal attachment diagnostics; does not create decals");

namespace {
bool TraceBlood() {
  static std::atomic<unsigned> records{0};
  return pdz_input::Supported() && REXCVAR_GET(pdz_blood_decal_trace) &&
      records.fetch_add(1, std::memory_order_relaxed) < 256;
}
}

REX_EXTERN(__imp__sub_82536F38);
REX_HOOK_RAW(sub_82536F38) {
  pdz_blood::Hit(ctx, base);
  if (TraceBlood())
    REXLOG_INFO("Blood diagnostic character hit: character={:08X} r4={:08X} r5={:08X} r6={:08X} head={}",
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u8);
  __imp__sub_82536F38(ctx, base);
}

REX_EXTERN(__imp__sub_8222CF70);
REX_HOOK_RAW(sub_8222CF70) {
  if (TraceBlood())
    REXLOG_INFO("Blood diagnostic decal request: manager={:08X} owner={:08X} part={} position={:08X} normal={:08X} type={} f1={} f2={}",
        ctx.r3.u32, ctx.r4.u32, ctx.r5.s32, ctx.r6.u32, ctx.r7.u32,
        ctx.r10.u32, ctx.f1.f64, ctx.f2.f64);
  __imp__sub_8222CF70(ctx, base);
}

REX_EXTERN(__imp__sub_8222D990);
REX_HOOK_RAW(sub_8222D990) {
  if (TraceBlood())
    REXLOG_INFO("Blood diagnostic accepted decal: group={:08X} position={:08X} normal={:08X} bone={}",
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r8.s32);
  __imp__sub_8222D990(ctx, base);
}

REX_EXTERN(__imp__sub_82220F78);
REX_HOOK_RAW(sub_82220F78) {
  // TU0 character initialization resolves these two effect resources. Match
  // identity only: never substitute arbitrary emitters or retain guest objects
  // across deaths/mission transitions. The game's existing blood gates still
  // decide whether this call happens at all.
  bool extra = false;
  if (pdz_input::Supported() && REXCVAR_GET(pdz_extra_blood)) {
    const auto resource = ctx.r4.u32;
    if (resource && (resource == REX_LOAD_U32(0x82E3B5FC) ||
                     resource == REX_LOAD_U32(0x82E3B600))) {
      static std::mutex mutex;
      static auto next = std::chrono::steady_clock::time_point{};
      const auto now = std::chrono::steady_clock::now();
      std::lock_guard lock(mutex);
      if (now >= next) {
        next = now + std::chrono::milliseconds(125);
        extra = true;
      }
    }
  }
  if (extra) {
    // Keep the native ABI, including stack arguments. The native helper clears
    // its output handle array before filling it; the following normal call
    // releases these temporary references exactly as the original hit caller
    // does. Emitter lifetime remains under the engine's effect manager.
    auto burst = ctx;
    __imp__sub_82220F78(burst, base);
  }
  __imp__sub_82220F78(ctx, base);
}
