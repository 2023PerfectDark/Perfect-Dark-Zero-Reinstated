#include "generated/default/pdzstocktu3_init.h"

#include <array>
#include <bit>
#include <cmath>
#include <mutex>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/runtime.h>
#include <rex/system/flags.h>
#include <rex/system/xmemory.h>

#include "game_cvars.h"
#include "game_constants.h"
#include "game_patches.h"
#include "mousehook.h"
#include "shader_loading.h"
#include "campaign_menu.h"

REX_EXTERN(__imp__sub_82561240);
REX_HOOK_RAW(sub_82561240)
{
  // Door root-motion and the completion state machine must advance together.
  // Run their native update at the original 30 Hz cadence, accumulating real
  // simulation time instead of dropping fractional high-FPS animation steps.
  struct Clock { uint32_t model=0, setup=0; double pending=0; };
  static thread_local std::unordered_map<uint32_t, Clock> clocks;
  const uint32_t door=ctx.r3.u32;
  const double dt=ctx.f1.f64;
  if (!door || !std::isfinite(dt) || dt<=0 || dt>.1) {
    clocks.erase(door);
    __imp__sub_82561240(ctx,base);
    return;
  }
  if(clocks.size()>4096)clocks.clear();
  auto& clock=clocks[door];
  const uint32_t model=REX_LOAD_U32(door+72),setup=REX_LOAD_U32(door+84);
  if(clock.model!=model||clock.setup!=setup)clock={model,setup,0};
  clock.pending+=dt;
  if(clock.pending+1e-9<1.0/30.0)return;
  const double elapsed=clock.pending;
  clock.pending=0;
  ctx.f1.f64=elapsed;
  __imp__sub_82561240(ctx,base);
}

REX_EXTERN(__imp__sub_8239FA38);
REX_HOOK_RAW(sub_8239FA38)
{
  // Only front-end intro/attract requests. Keep native initialization,
  // null-movie completion and title transitions; mission movies are untouched.
  const bool startup = (ctx.lr == 0x82626D40 && ctx.r3.u32 == 0x820CC344) ||
                       (ctx.lr == 0x8222D57C &&
                        (ctx.r3.u32 == 0x82096A00 || ctx.r3.u32 == 0x82096A1C));
  if (REXCVAR_GET(pdz_skip_intro) && startup &&
      REX_LOAD_U32(0x82E2F564) == 0)
  {
    REXLOG_INFO("Skip intro: bypassing startup movie {:08X}", ctx.r3.u32);
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_8239FA38(ctx, base);
}

REX_EXTERN(__imp__sub_823CFBA0);
REX_HOOK_RAW(sub_823CFBA0)
{
  // DarkXuiStartAttract's timer starts the demonstration after the title
  // screen is idle. Keep PRESS START waiting for real input when intro
  // skipping is enabled; preserve all other scene/input messages.
  const uint32_t message = ctx.r4.u32;
  if (REXCVAR_GET(pdz_skip_intro) && message &&
      REX_LOAD_U32(message + 4) == 39) {
    const uint32_t timer = REX_LOAD_U32(message + 16);
    if (timer && REX_LOAD_U32(timer) == 1235) {
      ctx.r3.u64 = 0;
      return;
    }
  }
  __imp__sub_823CFBA0(ctx, base);
}

namespace game_patches
{
  bool IsEnabled(PatchId patch)
  {
    switch (patch)
    {
    case PatchId::Fps60Unlock:
      return REXCVAR_GET(pdz_fps60_unlock);
    case PatchId::AspectRatio16_9:
      return REXCVAR_GET(pdz_aspect_ratio_16_9);
    }
    return false;
  }
} // namespace game_patches

namespace
{
  bool WriteGuestBytes(uint32_t address, const uint8_t *bytes, uint32_t size)
  {
    if (!bytes || !size)
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: empty patch data", address);
      return false;
    }

    auto *rt = rex::Runtime::instance();
    auto *memory = rt ? rt->memory() : nullptr;
    auto *heap = memory ? memory->LookupHeap(address) : nullptr;
    if (!heap)
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: no guest heap", address);
      return false;
    }

    const uint64_t last_address = static_cast<uint64_t>(address) + size - 1;
    if (last_address > UINT32_MAX ||
        address / heap->page_size() != last_address / heap->page_size())
    {
      REXLOG_WARN("Cannot apply game patch at guest address {:08X}: range crosses a guest page",
                  address);
      return false;
    }

    constexpr uint32_t writable = rex::memory::kMemoryProtectRead |
                                  rex::memory::kMemoryProtectWrite;
    uint32_t old_protect{};
    if (!heap->Protect(address, size, writable, &old_protect))
    {
      REXLOG_WARN("Cannot make guest range {:08X}+{:X} writable for game patch", address, size);
      return false;
    }

    std::memcpy(memory->TranslateVirtual(address), bytes, size);
    if (!heap->Protect(address, size, old_protect, nullptr))
    {
      REXLOG_WARN("Cannot restore guest protection for game patch at {:08X}", address);
      return false;
    }
    return true;
  }

  void ApplyAspectRatioPatch()
  {
    if (!game_patches::IsEnabled(game_patches::PatchId::AspectRatio16_9))
    {
      return;
    }

    const auto patch = GameConstants::PatchConstants::AspectRatio();
    const std::array<uint8_t, 4> bytes{
        static_cast<uint8_t>(patch.value >> 24),
        static_cast<uint8_t>(patch.value >> 16),
        static_cast<uint8_t>(patch.value >> 8),
        static_cast<uint8_t>(patch.value)};
    WriteGuestBytes(static_cast<uint32_t>(patch.address), bytes.data(),
                    static_cast<uint32_t>(bytes.size()));
  }
} // namespace

REX_EXTERN(__imp__sub_826DE408);

REX_HOOK_RAW(sub_826DE408)
{
  const auto patch = GameConstants::PatchConstants::Fps60();
  if (ctx.lr == patch.call_site &&
      game_patches::IsEnabled(game_patches::PatchId::Fps60Unlock))
  {
    ctx.r7.u32 = (ctx.r7.u32 & patch.preserve_mask) | patch.set_bits;
  }

  __imp__sub_826DE408(ctx, base);
}

REX_EXTERN(__imp__sub_8239CBD8);

// The SDK's XGetLanguage ignores user_language; a missing loc folder falls back to English.
REX_HOOK_RAW(sub_8239CBD8)
{
  const auto patch = GameConstants::PatchConstants::Language();
  if (ctx.lr == patch.call_site && base)
  {
    uint32_t language = REXCVAR_GET(user_language);
    if (language < 1 || language > patch.max_language)
    {
      language = 1;
    }
    rex::memory::store_and_swap<uint32_t>(base + patch.address, language);
  }

  __imp__sub_8239CBD8(ctx, base);
}

REX_EXTERN(__imp__sub_82AD4A18);

REX_EXTERN(__imp__sub_824F5830);
REX_HOOK_RAW(sub_824F5830)
{
  // The original add routine dereferences a null free-list entry if the
  // 13-object pool is exhausted. The size cheat must never bypass this bound.
  if (pdz_input::InventoryStorageFull(ctx.r3.u32)) {
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_824F5830(ctx, base);
}

REX_HOOK_RAW(sub_82AD4A18)
{
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t state_address = ctx.r4.u32;
  __imp__sub_82AD4A18(ctx, base);
  if ((user_index == 0 || (user_index & 0xFF) == 0xFF) && base && state_address && ctx.r3.u32 == 0)
    pdz_input::Poll(base + state_address);
}
namespace game_patches
{
  void ApplyEnabledPatches()
  {
    pdz_input::ValidateImage();
    ApplyAspectRatioPatch();
    UpdateFov();
  }

  void UpdateFov()
  {
    if (!pdz_input::Supported()) return;
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    const float fov = static_cast<float>(REXCVAR_GET(pdz_fov));
    if (!std::isfinite(fov) || fov < 40.f || fov > 120.f) return;
    static float applied = -1.f;
    if (applied == fov) return;
    const auto bits = std::bit_cast<uint32_t>(fov);
    const std::array<uint8_t, 4> bytes{uint8_t(bits >> 24), uint8_t(bits >> 16),
                                      uint8_t(bits >> 8), uint8_t(bits)};
    if (WriteGuestBytes(0x820EAF40, bytes.data(), 4)) {
      applied = fov;
      REXLOG_INFO("PDZ FOV scale applied: {}", fov);
    }
  }
} // namespace game_patches

REX_EXTERN(__imp__sub_8252FAB0);
REX_HOOK_RAW(sub_8252FAB0)
{
  // At this call site in sub_825B31D0, f17 is the smoothed ADS pose weight.
  // Later code uses it for the weapon's own +140/+144/+148 position offsets
  // and (1-f17) for hip yaw/sway. Override only this local-player render path;
  // do not change shared weapon definitions, gameplay aim flags, or FOV.
  const bool force = ctx.lr == 0x825B34C4 && pdz_input::ForceAimPose(ctx.r31.u32);
  __imp__sub_8252FAB0(ctx, base);
  if (force) ctx.f17.f64 = 1.0;
}

REX_EXTERN(__imp__sub_82AD4A78);
REX_HOOK_RAW(sub_82AD4A78) {
  const auto user = ctx.r3.u32;
  const auto flags = ctx.r4.u32;
  const auto out = ctx.r5.u32;
  __imp__sub_82AD4A78(ctx, base);
  if (ctx.r3.u32 == 4306 && (!(flags & 0xFF) || (flags & 1)) &&
      pdz_input::MenuKeystroke((flags & 0x40000000) ? 0 : user, out)) ctx.r3.u64 = 0;
  if (ctx.r3.u32 == 0 && out && pdz_campaign::BackKey(REX_LOAD_U16(out), REX_LOAD_U16(out + 4)))
    ctx.r3.u64 = 4306;
}
REX_EXTERN(__imp__sub_82AD4A58);
REX_HOOK_RAW(sub_82AD4A58) {
  const auto user_ptr = ctx.r3.u32;
  const auto flags = ctx.r4.u32;
  const auto out = ctx.r5.u32;
  const auto user = REX_LOAD_U32(user_ptr);
  __imp__sub_82AD4A58(ctx, base);
  if (ctx.r3.u32 == 4306 && (!(flags & 0xFF) || (flags & 1)) &&
      pdz_input::MenuKeystroke((flags & 0x40000000) ? 0 : user, out)) {
    REX_STORE_U32(user_ptr, 0); ctx.r3.u64 = 0;
  }
  if (ctx.r3.u32 == 0 && out && pdz_campaign::BackKey(REX_LOAD_U16(out), REX_LOAD_U16(out + 4)))
    ctx.r3.u64 = 4306;
}
REX_EXTERN(__imp__sub_826DF960);
REX_HOOK_RAW(sub_826DF960) {
  pdz_shaders::Wait();
  pdz_input::PaceFrame();
  __imp__sub_826DF960(ctx, base);
}

REX_EXTERN(__imp__sub_823F58F8);
REX_HOOK_RAW(sub_823F58F8) {
  uint32_t item = 0;
  if (pdz_input::InventoryPageItem(ctx.r3.u32, ctx.r4.u32, item)) {
    ctx.r3.u64 = item;
    return;
  }
  __imp__sub_823F58F8(ctx, base);
}
