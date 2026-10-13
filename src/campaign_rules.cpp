#include "weapon_challenge.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "campaign_logic.h"
#include "campaign_damage.h"
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
  const auto session = REX_LOAD_U32(0x82D6B2B0);
  // Only solo campaign. Network and Combat Arena retain their native rules.
  if (!Readable(session, 40) || REX_LOAD_U32(session + 36) != 0) return 0;
  return REXCVAR_GET(pdz_campaign_rules);
}
float ReadFloat(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(REX_LOAD_U32(address));
}
void StoreFloat(uint8_t* base, uint32_t address, float value) {
  REX_STORE_U32(address, std::bit_cast<uint32_t>(value));
}
bool LocalPlayer(uint8_t* base, uint32_t player) {
  const uint32_t local = REX_LOAD_U32(0x82E35468);
  return player && Readable(local, 956) && REX_LOAD_U32(local + 952) == player;
}
void RecoverCheckpoint(PPCContext& ctx, uint8_t* base, uint32_t player,
                       uint32_t health) {
  const float current = ReadFloat(base, health + 968);
  const float recovered = pdz_campaign::CheckpointHealth(
      current, ReadFloat(base, health + 972));
  if (!(recovered > current)) return;
  PPCContext call = ctx;
  call.r3.u64 = player;
  call.r5.u64 = 0;
  call.f1.f64 = recovered;
  sub_82599C50(call, base); // Native current-health setter; ceiling is untouched.
  const uint32_t hud = REX_LOAD_U32(player + 6580);
  if (Readable(hud, 64)) {
    call.r3.u64 = hud;
    sub_82260C38(call, base); // Show the recovered amount through the native HUD.
  }
}
}


// The level-state constructor clears the checkpoint flag on every mission load,
// including retries. Reset here even if the allocator reuses the player address.
REX_EXTERN(__imp__sub_82148DD8);
REX_HOOK_RAW(sub_82148DD8) {
  if (pdz_input::Supported()) { checkpoint.ResetMission(); pdz_challenge::Reset(); }
  __imp__sub_82148DD8(ctx, base);
}

REX_EXTERN(__imp__sub_82597FC0);
REX_HOOK_RAW(sub_82597FC0) {
  const int rules = Rules(base);
  const uint32_t player = ctx.r3.u32;
  // Enforce the challenge from the player update, independently of rendering.
  pdz_challenge::Tick(ctx,base,player);
  if (pdz_input::Supported() && LocalPlayer(base, player) && Readable(player, 6584)) {
    const bool reached = checkpoint.Observe((REX_LOAD_U8(0x82E41930) & 0x40) != 0);
    const uint32_t health = REX_LOAD_U32(player + 6560);
    if (rules == 2 && Readable(health, 2012)) {
      if (reached) RecoverCheckpoint(ctx, base, player, health);
      StoreFloat(base, health + 2004, pdz_campaign::AdvanceShockTimer(
          ReadFloat(base, health + 2004), float(ctx.f1.f64)));
    }
  }
  if (rules == 2) return; // Classic recovers shock only at a checkpoint edge.
  if (rules == 1 && Readable(player, 6564)) {
    const uint32_t health = REX_LOAD_U32(player + 6560);
    if (Readable(health, 2012)) {
      const float current = std::bit_cast<float>(REX_LOAD_U32(health + 968));
      const float ceiling = std::bit_cast<float>(REX_LOAD_U32(health + 972));
      if (std::isfinite(current) && current > 0 && current <= 100 &&
          std::isfinite(ceiling) && ceiling >= 0 && ceiling <= 100) {
        // Retain native delay, recovery effects and setter; allow full recovery.
        REX_STORE_U32(health + 972, std::bit_cast<uint32_t>(100.f));
        __imp__sub_82597FC0(ctx, base);
        REX_STORE_U32(health + 972, std::bit_cast<uint32_t>(ceiling));
        return;
      }
    }
  }
  __imp__sub_82597FC0(ctx, base);
}

REX_EXTERN(__imp__sub_822609F0);
REX_HOOK_RAW(sub_822609F0) {
  const uint32_t hud = ctx.r3.u32;
  if (Rules(base) == 2 && Readable(hud, 64)) {
    const uint32_t health = REX_LOAD_U32(hud + 16);
    if (Readable(health, 2012)) {
      const uint32_t player = REX_LOAD_U32(health + 952);
      if (LocalPlayer(base, player) && Readable(player, 6584) &&
          REX_LOAD_U32(player + 6580) == hud && REX_LOAD_U8(hud) &&
          pdz_campaign::HideClassicHealth(ReadFloat(base, hud + 4), float(ctx.f1.f64))) {
        // Same active flag cleared by the native six-second/fully-recovered
        // branch. Let the original HUD finish its normal fade/effects. Damage
        // and checkpoint recovery reactivate it through sub_82260C38.
        REX_STORE_U8(hud, 0);
      }
    }
  }
  __imp__sub_822609F0(ctx, base);
}

REX_EXTERN(__imp__sub_823D5CF8);
REX_HOOK_RAW(sub_823D5CF8) {
  __imp__sub_823D5CF8(ctx, base);
  if (Rules(base) == 2 && ctx.r3.u32 == 1) ctx.r3.u64 = 0;
}

REX_EXTERN(__imp__sub_823D6440);
REX_HOOK_RAW(sub_823D6440) {
  if (Rules(base) == 2) {
    ctx.r4.u64 = 1; // Hide checkpoint entry through its native visibility helper.
    sub_823A36D8(ctx, base);
    return;
  }
  __imp__sub_823D6440(ctx, base);
}

REX_EXTERN(__imp__sub_823D64A8);
REX_HOOK_RAW(sub_823D64A8) {
  if (Rules(base) == 2) {
    // Stale focus/controller activation cannot bypass the Classic rule.
    const uint32_t session = REX_LOAD_U32(0x82D6B2B0);
    REX_STORE_U8(session + 24, 0);
    sub_823D5EC0(ctx, base);
    return;
  }
  __imp__sub_823D64A8(ctx, base);
}

REX_EXTERN(__imp__sub_823D5EC0);
REX_HOOK_RAW(sub_823D5EC0) {
  if (Rules(base) == 2) REX_STORE_U8(REX_LOAD_U32(0x82D6B2B0) + 24, 0);
  __imp__sub_823D5EC0(ctx, base);
}

// Scope the exception to this impact and victim, never an attacker's equipped gun.
namespace { thread_local uint32_t sniperVictim = 0; }
REX_EXTERN(__imp__sub_82539980);
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
      sub_821ED078(call, base);
      const auto resource = REX_LOAD_U32(out);
      if (Readable(resource, 8)) {
        call.r3.u64 = resource;
        sub_826478B8(call, base);
        if (call.r3.u32 && call.r3.u32 == REX_LOAD_U32(weapon + 860))
          sniperVictim = victim;
      }
    }
  }
  __imp__sub_82539980(ctx, base);
  sniperVictim = previous;
}

REX_EXTERN(__imp__sub_8259B738);
REX_HOOK_RAW(sub_8259B738) {
  const auto player = ctx.r3.u32;
  if (Rules(base) == 1 && LocalPlayer(base, player) && Readable(player, 6564)) {
    const auto health = REX_LOAD_U32(player + 6560);
    if (Readable(health, 980)) {
      const auto damage = pdz_campaign::ModernShieldDamage(
          float(ctx.f1.f64), float(ctx.f2.f64), ReadFloat(base, health + 976),
          sniperVictim == player);
      ctx.f1.f64 = damage.health;
      ctx.f2.f64 = damage.shield;
    }
  }
  // Preserve native death processing, recovery delay, armor setter and HUD.
  __imp__sub_8259B738(ctx, base);
}
