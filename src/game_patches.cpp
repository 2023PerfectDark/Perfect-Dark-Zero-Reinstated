#include "generated/default/perfectdarkzerorecomp_init.h"

#include <array>
#include <bit>
#include <cmath>
#include <mutex>
#include <cstdint>
#include <cstring>

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
#include "campaign_menu.h"

REX_EXTERN(__imp__sub_823975C0);
REX_HOOK_RAW(sub_823975C0)
{
  // Only front-end intro/attract requests. Keep native initialization,
  // null-movie completion and title transitions; mission movies are untouched.
  const bool startup = (ctx.lr == 0x826173CC && ctx.r3.u32 == 0x820CD6D4) ||
                       (ctx.lr == 0x822262B4 &&
                        (ctx.r3.u32 == 0x82098800 || ctx.r3.u32 == 0x8209881C));
  if (REXCVAR_GET(pdz_skip_intro) && startup &&
      REX_LOAD_U32(0x82E2E794) == 0)
  {
    REXLOG_INFO("Skip intro: bypassing startup movie {:08X}", ctx.r3.u32);
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_823975C0(ctx, base);
}

REX_EXTERN(__imp__sub_823C6360);
REX_HOOK_RAW(sub_823C6360)
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
  __imp__sub_823C6360(ctx, base);
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

REX_EXTERN(__imp__sub_826CE6B8);

REX_HOOK_RAW(sub_826CE6B8)
{
  const auto patch = GameConstants::PatchConstants::Fps60();
  if (ctx.lr == patch.call_site &&
      game_patches::IsEnabled(game_patches::PatchId::Fps60Unlock))
  {
    ctx.r7.u32 = (ctx.r7.u32 & patch.preserve_mask) | patch.set_bits;
  }

  __imp__sub_826CE6B8(ctx, base);
}

REX_EXTERN(__imp__sub_82394750);

// The SDK's XGetLanguage ignores user_language; a missing loc folder falls back to English.
REX_HOOK_RAW(sub_82394750)
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

  __imp__sub_82394750(ctx, base);
}

REX_EXTERN(__imp__sub_82AC4CB8);

REX_EXTERN(__imp__sub_824E84B8);
REX_HOOK_RAW(sub_824E84B8)
{
  // The original add routine dereferences a null free-list entry if the
  // 13-object pool is exhausted. The size cheat must never bypass this bound.
  if (pdz_input::InventoryStorageFull(ctx.r3.u32)) {
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_824E84B8(ctx, base);
}

REX_HOOK_RAW(sub_82AC4CB8)
{
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t state_address = ctx.r4.u32;
  __imp__sub_82AC4CB8(ctx, base);
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
    if (WriteGuestBytes(0x820EC228, bytes.data(), 4)) {
      applied = fov;
      REXLOG_INFO("PDZ FOV scale applied: {}", fov);
    }
  }
} // namespace game_patches

REX_EXTERN(__imp__sub_82521240);
REX_HOOK_RAW(sub_82521240)
{
  // At this call site in sub_825A39B8, f17 is the smoothed ADS pose weight.
  // Later code uses it for the weapon's own +140/+144/+148 position offsets
  // and (1-f17) for hip yaw/sway. Override only this local-player render path;
  // do not change shared weapon definitions, gameplay aim flags, or FOV.
  const bool force = ctx.lr == 0x825A3CB4 && pdz_input::ForceAimPose(ctx.r31.u32);
  __imp__sub_82521240(ctx, base);
  if (force) ctx.f17.f64 = 1.0;
}

REX_EXTERN(__imp__sub_82AC4D18);
REX_HOOK_RAW(sub_82AC4D18) {
  const auto user = ctx.r3.u32;
  const auto flags = ctx.r4.u32;
  const auto out = ctx.r5.u32;
  __imp__sub_82AC4D18(ctx, base);
  if (ctx.r3.u32 == 4306 && (!(flags & 0xFF) || (flags & 1)) &&
      pdz_input::MenuKeystroke((flags & 0x40000000) ? 0 : user, out)) ctx.r3.u64 = 0;
  if (ctx.r3.u32 == 0 && out && pdz_campaign::BackKey(REX_LOAD_U16(out), REX_LOAD_U16(out + 4)))
    ctx.r3.u64 = 4306;
}
REX_EXTERN(__imp__sub_82AC4CF8);
REX_HOOK_RAW(sub_82AC4CF8) {
  const auto user_ptr = ctx.r3.u32;
  const auto flags = ctx.r4.u32;
  const auto out = ctx.r5.u32;
  const auto user = REX_LOAD_U32(user_ptr);
  __imp__sub_82AC4CF8(ctx, base);
  if (ctx.r3.u32 == 4306 && (!(flags & 0xFF) || (flags & 1)) &&
      pdz_input::MenuKeystroke((flags & 0x40000000) ? 0 : user, out)) {
    REX_STORE_U32(user_ptr, 0); ctx.r3.u64 = 0;
  }
  if (ctx.r3.u32 == 0 && out && pdz_campaign::BackKey(REX_LOAD_U16(out), REX_LOAD_U16(out + 4)))
    ctx.r3.u64 = 4306;
}
REX_EXTERN(__imp__sub_826CFC10);
REX_HOOK_RAW(sub_826CFC10) {
  pdz_input::PaceFrame();
  __imp__sub_826CFC10(ctx, base);
}

REX_EXTERN(__imp__sub_823EBA08);
REX_HOOK_RAW(sub_823EBA08) {
  uint32_t item = 0;
  if (pdz_input::InventoryPageItem(ctx.r3.u32, ctx.r4.u32, item)) {
    ctx.r3.u64 = item;
    return;
  }
  __imp__sub_823EBA08(ctx, base);
}
