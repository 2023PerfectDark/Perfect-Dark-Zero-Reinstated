#include "weapon_challenge.h"
#include "generated/default/pdzstocktu3_init.h"
#include "campaign_logic.h"
#include "debug_checkpoint_trace.h"
#include "debug_position.h"
#include "campaign_damage.h"
#include "perfect_dark_zero_mode.h"
#include <map>
#include <algorithm>
#include <cstring>
#include "game_cvars.h"
#include "mousehook.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <bit>
#include <cmath>
#include <SDL3/SDL.h>
#include <rex/logging.h>

namespace {
pdz_campaign::CheckpointRecovery checkpoint;
bool Readable(uint32_t address, uint32_t length) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !address || uint64_t(address) + length > 0x100000000ull) return false;
  for (uint64_t p = address; p < uint64_t(address) + length;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(p));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(p), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    p = (p / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
int Rules(uint8_t* base) {
  if (!pdz_input::Supported()) return 0;
  const auto session = REX_LOAD_U32(0x82D6BFD8);
  // Only solo campaign. Network and Combat Arena retain their native rules.
  if (!Readable(session, 44) || REX_LOAD_U32(session + 40) != 0) return 0;
  return REXCVAR_GET(pdz_campaign_rules);
}
float ReadFloat(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(REX_LOAD_U32(address));
}
void StoreFloat(uint8_t* base, uint32_t address, float value) {
  REX_STORE_U32(address, std::bit_cast<uint32_t>(value));
}
bool LocalPlayer(uint8_t* base, uint32_t player) {
  const uint32_t local = REX_LOAD_U32(0x82E36228);
  return player && Readable(local, 968) && REX_LOAD_U32(local + 964) == player;
}
void RecoverCheckpoint(PPCContext& ctx, uint8_t* base, uint32_t player,
                       uint32_t health) {
  const float current = ReadFloat(base, health + 980);
  const float recovered = pdz_campaign::CheckpointHealth(
      current, ReadFloat(base, health + 984));
  if (!(recovered > current)) return;
  PPCContext call = ctx;
  call.r3.u64 = player;
  call.r5.u64 = 0;
  call.f1.f64 = recovered;
  sub_825A9468(call, base); // Native current-health setter; ceiling is untouched.
  const uint32_t hud = REX_LOAD_U32(player + 6580);
  if (Readable(hud, 64)) {
    call.r3.u64 = hud;
    sub_82268030(call, base); // Show the recovered amount through the native HUD.
  }
}
}


// The level-state constructor clears the checkpoint flag on every mission load,
// including retries. Reset here even if the allocator reuses the player address.
REX_EXTERN(__imp__sub_8214D888);
REX_HOOK_RAW(sub_8214D888) {
  if (pdz_input::Supported()) { checkpoint.ResetMission(); pdz_challenge::Reset(); pdz_position::ResetLevel(); }
  __imp__sub_8214D888(ctx, base);
}

REX_EXTERN(__imp__sub_825A7848);
REX_HOOK_RAW(sub_825A7848) {
  const int rules = Rules(base);
  const uint32_t player = ctx.r3.u32;
  // Enforce the challenge from the player update, independently of rendering.
  pdz_challenge::Tick(ctx,base,player);
  pdz_position::Tick(ctx,base,player);
  if (pdz_input::Supported() && LocalPlayer(base, player) && Readable(player, 6584)) {
    pdz_checkpoint::Observe(base);
    const bool reached = checkpoint.Observe((REX_LOAD_U8(0x82E426F8) & 0x40) != 0);
    const uint32_t health = REX_LOAD_U32(player + 6560);
    if (rules == 2 && Readable(health, 2580)) {
      if (reached) RecoverCheckpoint(ctx, base, player, health);
      StoreFloat(base, health + 2572, pdz_campaign::AdvanceShockTimer(
          ReadFloat(base, health + 2572), float(ctx.f1.f64)));
    }
  }
  if (rules == 2) return; // Classic recovers shock only at a checkpoint edge.
  if (rules == 1 && Readable(player, 6564)) {
    const uint32_t health = REX_LOAD_U32(player + 6560);
    if (Readable(health, 2580)) {
      const float current = std::bit_cast<float>(REX_LOAD_U32(health + 980));
      const float ceiling = std::bit_cast<float>(REX_LOAD_U32(health + 984));
      if (std::isfinite(current) && current > 0 && current <= 100 &&
          std::isfinite(ceiling) && ceiling >= 0 && ceiling <= 100) {
        // Retain native delay, recovery effects and setter; allow full recovery.
        REX_STORE_U32(health + 984, std::bit_cast<uint32_t>(100.f));
        __imp__sub_825A7848(ctx, base);
        REX_STORE_U32(health + 984, std::bit_cast<uint32_t>(ceiling));
        return;
      }
    }
  }
  __imp__sub_825A7848(ctx, base);
}

REX_EXTERN(__imp__sub_82267DE0);
REX_HOOK_RAW(sub_82267DE0) {
  const uint32_t hud = ctx.r3.u32;
  if (Rules(base) == 2 && Readable(hud, 64)) {
    const uint32_t health = REX_LOAD_U32(hud + 16);
    if (Readable(health, 2580)) {
      const uint32_t player = REX_LOAD_U32(health + 964);
      if (LocalPlayer(base, player) && Readable(player, 6584) &&
          REX_LOAD_U32(player + 6580) == hud && REX_LOAD_U8(hud) &&
          pdz_campaign::HideClassicHealth(ReadFloat(base, hud + 4), float(ctx.f1.f64))) {
        // Same active flag cleared by the native six-second/fully-recovered
        // branch. Let the original HUD finish its normal fade/effects. Damage
        // and checkpoint recovery reactivate it through sub_82268030.
        REX_STORE_U8(hud, 0);
      }
    }
  }
  __imp__sub_82267DE0(ctx, base);
}

REX_EXTERN(__imp__sub_823DFA40);
REX_HOOK_RAW(sub_823DFA40) {
  __imp__sub_823DFA40(ctx, base);
  if (Rules(base) == 2 && ctx.r3.u32 == 1) ctx.r3.u64 = 0;
}

REX_EXTERN(__imp__sub_823E0170);
REX_HOOK_RAW(sub_823E0170) {
  if (Rules(base) == 2) {
    ctx.r4.u64 = 1; // Hide checkpoint entry through its native visibility helper.
    sub_823ABDB8(ctx, base);
    return;
  }
  __imp__sub_823E0170(ctx, base);
}

REX_EXTERN(__imp__sub_823E01D8);
REX_HOOK_RAW(sub_823E01D8) {
  if (Rules(base) == 2) {
    // Stale focus/controller activation cannot bypass the Classic rule.
    const uint32_t session = REX_LOAD_U32(0x82D6BFD8);
    REX_STORE_U8(session + 24, 0);
    sub_823DFC08(ctx, base);
    return;
  }
  __imp__sub_823E01D8(ctx, base);
}

REX_EXTERN(__imp__sub_823DFC08);
REX_HOOK_RAW(sub_823DFC08) {
  if (Rules(base) == 2) REX_STORE_U8(REX_LOAD_U32(0x82D6BFD8) + 24, 0);
  __imp__sub_823DFC08(ctx, base);
}

// Scope the exception to this impact and victim, never an attacker's equipped gun.
namespace { thread_local uint32_t sniperVictim = 0; }
REX_EXTERN(__imp__sub_82549040);
void pdz_campaign::BulletDamage(PPCContext& ctx, uint8_t* base) {
  const auto previous = sniperVictim;
  sniperVictim = 0;
  const auto victim = ctx.r3.u32, weapon = ctx.r4.u32;
  if (Rules(base) == 1 && LocalPlayer(base, victim) && Readable(weapon, 864) &&
      REX_LOAD_U32(weapon + 12) == 4) {
    PPCContext call = ctx;
    call.r1.u32 -= 1024;
    if (Readable(call.r1.u32, 1024)) {
      const auto text = call.r1.u32 + 256, out = call.r1.u32 + 128;
      constexpr char name[] = "aid_xml_guns_sniperrifle";
      std::memcpy(base + text, name, sizeof(name));
      REX_STORE_U32(out, 0);
      call.r3.u64 = text; call.r4.u64 = out;
      sub_821F4148(call, base);
      const auto resource = REX_LOAD_U32(out);
      if (Readable(resource, 8)) {
        call.r3.u64 = resource;
        sub_82657700(call, base);
        if (call.r3.u32 && call.r3.u32 == REX_LOAD_U32(weapon + 860))
          sniperVictim = victim;
      }
    }
  }
  __imp__sub_82549040(ctx, base);
  sniperVictim = previous;
}

REX_EXTERN(__imp__sub_825AAF50);
REX_HOOK_RAW(sub_825AAF50) {
  pdz_custom::ScalePlayerDamage(ctx,base);
  const auto player = ctx.r3.u32;
  if (Rules(base) == 1 && LocalPlayer(base, player) && Readable(player, 6564)) {
    const auto health = REX_LOAD_U32(player + 6560);
    if (Readable(health, 992)) {
      const auto damage = pdz_campaign::ModernShieldDamage(
          float(ctx.f1.f64), float(ctx.f2.f64), ReadFloat(base, health + 988),
          sniperVictim == player);
      ctx.f1.f64 = damage.health;
      ctx.f2.f64 = damage.shield;
    }
  }
  // Preserve native death processing, recovery delay, armor setter and HUD.
  __imp__sub_825AAF50(ctx, base);
}

#include "perfect_dark_zero_runtime.inl"
