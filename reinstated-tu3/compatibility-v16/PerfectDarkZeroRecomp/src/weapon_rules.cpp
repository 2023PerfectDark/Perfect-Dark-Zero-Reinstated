#include "weapon_challenge.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "weapon_rules.h"
#include "mod/breathing_motion.h"
#include "reload_timing_policy.h"
#include "game_cvars.h"
#include "mousehook.h"
#include <bit>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <array>
#include <mutex>
#include <chrono>
#include <rex/logging.h>
#include "surface_runtime.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

namespace {
std::mutex sway_mutex;
std::atomic<uint64_t> footstep_sequence{0};
std::atomic<int64_t> last_footstep_ms{0};
struct MovementSway {uint32_t player=0;bool active=false,moving=false;float sideways=0;std::chrono::steady_clock::time_point time{};} movement_sway;
std::atomic<uint32_t> inspection_weapon{0}, inspection_offhand{0};
struct InspectionPlayback { uint32_t action=0, controller=0; float progress=0; unsigned kind=0, animation=23; } inspection_playback;
thread_local uint32_t inspection_sampling=0;
thread_local float inspection_sample_progress=0;
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
  if (!Readable(session, 40) || REX_LOAD_U32(session + 36) != 0) return 0;
  return REXCVAR_GET(pdz_campaign_rules);
}
bool LocalPlayer(uint8_t* base, uint32_t player) {
  const auto local = REX_LOAD_U32(0x82E35468);
  return player && Readable(local, 956) && REX_LOAD_U32(local + 952) == player;
}
bool LocalWeaponAction(uint8_t* base, uint32_t action) {
  if (!Readable(action, 32)) return false;
  const auto weapon = REX_LOAD_U32(action + 28);
  return Readable(weapon, 96) && LocalPlayer(base, REX_LOAD_U32(weapon + 92));
}
int ReadyAnimationId(uint8_t* base, uint32_t action) {
  if (!pdz_input::Supported() || !LocalWeaponAction(base, action)) return -1;
  const auto weapon=REX_LOAD_U32(action+28);
  if (!Readable(weapon,1284)) return -1;
  const auto player=REX_LOAD_U32(weapon+92);
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if (!Readable(player,0x16A4) || !Readable(session,40) ||
      REX_LOAD_U32(session+36)!=0 || REX_LOAD_U8(session) ||
      REX_LOAD_U8(player+0x16A3) || pdz_surface::IntroCameraActive() ||
      REX_LOAD_U32(player+4088)==2 ||
      (REX_LOAD_U32(player+4076)!=weapon && REX_LOAD_U32(player+4080)!=weapon) ||
      REX_LOAD_U32(weapon+1280)!=action) return -1;
  const auto definition=REX_LOAD_U32(weapon+860);
  const auto animations=REX_LOAD_U32(weapon+1248);
  if (!Readable(definition,1148) || !Readable(animations,220) ||
      !REX_LOAD_U32(animations+108) || !REX_LOAD_U32(animations+216)) return -1;
  const auto id=REX_LOAD_U32(definition+1144);
  return id<64 ? int(id) : -1;
}
}

namespace pdz_weapons {
uint64_t FootstepSequence(){return footstep_sequence.load(std::memory_order_relaxed);}
void SetMovementSway(uint32_t player,bool active,bool moving,float sideways){
  std::lock_guard lock(sway_mutex);
  movement_sway={player,active,moving,sideways,std::chrono::steady_clock::now()};
}
bool SlowBreathingRecovery(uint8_t* base,uint32_t player) {
  if(!Readable(player,4084))return false;
  for(unsigned offset:{4076u,4080u}) {
    const auto weapon=REX_LOAD_U32(player+offset);
    if(!Readable(weapon,864))continue;
    const auto definition=REX_LOAD_U32(weapon+860);
    if(!Readable(definition,1148))continue;
    const auto id=REX_LOAD_U32(definition+1144);
    if(id<64&&breathing_motion::SlowRecoveryWeapon(id))return true;
  }
  return false;
}
void RequestInspection(uint32_t weapon, uint32_t offhand) {
  inspection_weapon.store(weapon);
  inspection_offhand.store(offhand != weapon ? offhand : 0);
}
void ClearInspection() { inspection_weapon.store(0); inspection_offhand.store(0); }
uint32_t SecondaryHudSlots(uint8_t* base, uint32_t native) {
  // The native segmented secondary-ammo gauge subtracts a pixel for every
  // segment. Large Classic reserves exhaust its width. Its one-slot path is
  // already a continuous gauge using the weapon's real fill fraction.
  return (Rules(base) == 2 || pdz_challenge::Limit(base)) && native > 64 ? 1 : native;
}
uint32_t ReserveLimit(uint8_t* base, uint32_t record) {
  const auto native = REX_LOAD_U8(record);
  // Capacity consumers use a 16-bit result. Do not truncate 800 into the
  // byte-sized asset record or alter its difficulty-specific starting ammo.
  if (native && pdz_challenge::Limit(base)) return pdz_challenge::Limit(base);
  return native && Rules(base) == 2 ? 800 : native;
}
double Spread(uint8_t* base, uint32_t player, double original) {
  return Rules(base) != 0 && REXCVAR_GET(pdz_campaign_no_spread) &&
      LocalPlayer(base, player) ? 0.0 : original;
}
}

// This call builds the held gun's movement rotation, before recoil/action
// transforms are composed. Never modify shared weapon definitions or aim.
REX_EXTERN(__imp__sub_826F4828);
REX_HOOK_RAW(sub_826F4828) {
  if(ctx.lr==0x825A3DA0&&pdz_input::CrosshairVisible()&&REXCVAR_GET(head_roll)&&REXCVAR_GET(pdz_campaign_rules)){
    MovementSway sway;{std::lock_guard lock(sway_mutex);sway=movement_sway;}
    if(sway.active&&sway.player==ctx.r31.u32&&std::chrono::steady_clock::now()-sway.time<std::chrono::milliseconds(150)){
      const double horizontal=sway.moving?std::abs(sway.sideways):1.;
      if(std::isfinite(ctx.f1.f64))ctx.f1.f64*=2.4;
      if(std::isfinite(ctx.f2.f64))ctx.f2.f64*=2.4*horizontal;
    }
  }
  __imp__sub_826F4828(ctx,base);
}

// Native positional cue constructor: r3 contains the cue name, r5 its actor.
// Only Joanna's FSTEP cues drive her camera, never nearby NPC footsteps.
REX_EXTERN(__imp__sub_821F70D0);
REX_HOOK_RAW(sub_821F70D0){
  bool step=false;
  if(pdz_input::Supported()){
    const auto camera=REX_LOAD_U32(0x82D68320);
    const auto player=Readable(camera,12)?REX_LOAD_U32(camera+8):0;
    const auto cue=ctx.r3.u32;
    if(player&&(ctx.r5.u32==player||ctx.r4.u32==player+48)&&Readable(cue,6)){
      constexpr char prefix[]="fstep_";step=true;
      for(unsigned i=0;i<6;++i){auto c=REX_LOAD_U8(cue+i);if(c>='A'&&c<='Z')c+=32;if(c!=prefix[i])step=false;}
    }
  }
  __imp__sub_821F70D0(ctx,base);
  if(step&&ctx.r3.u32){
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous=last_footstep_ms.load();
    if(now-previous>80&&last_footstep_ms.compare_exchange_strong(previous,now))footstep_sequence.fetch_add(1,std::memory_order_relaxed);
  }
}

REX_EXTERN(__imp__sub_822EF0A8);
REX_HOOK_RAW(sub_822EF0A8) {
  // Only these two DataCore update calls are its timed training refill.
  // Normal pickups, reloads and starting ammunition keep their native paths.
  if (pdz_input::Supported() && !REXCVAR_GET(pdz_training_ammo_refill) &&
      (ctx.lr == 0x822F44B4 || ctx.lr == 0x822F44EC)) return;
  __imp__sub_822EF0A8(ctx, base);
}

REX_EXTERN(__imp__sub_82244A10);
REX_HOOK_RAW(sub_82244A10) {
  // FAC-16 event 1 sets action+88 to one second when zoomed (82244DA0).
  // This query stops the native held-trigger loop in 822339B0/82233DF8.
  // Ignore that zoom-only lockout, not the primary cadence at definition+192.
  const auto action = ctx.r3.u32;
  if (Rules(base) != 0 && REXCVAR_GET(pdz_campaign_zoom_auto) &&
      LocalWeaponAction(base, action) && REX_LOAD_U32(action) == 0x8209A328) {
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_82244A10(ctx, base);
}

#include "weapon_wall_raise.inl"
REX_EXTERN(__imp__sub_82232D58);
REX_HOOK_RAW(sub_82232D58) {
  const auto action = ctx.r3.u32;
  bool inspect=false;
  if (pdz_input::Supported() && Readable(action, 64)) {
    const auto weapon = REX_LOAD_U32(action + 28);
    if (weapon && (inspection_weapon.load() == weapon || inspection_offhand.load() == weapon) && Readable(weapon, 1284) &&
        REX_LOAD_U32(weapon + 1280) == action) {
      // Consume each hand independently; never queue behind reloads.
      auto expected=weapon; inspection_weapon.compare_exchange_strong(expected,0);
      expected=weapon; inspection_offhand.compare_exchange_strong(expected,0);
      const auto player = REX_LOAD_U32(weapon + 92);
      const auto session = REX_LOAD_U32(0x82D6B2B0);
      if (LocalPlayer(base, player) && Readable(player, 0x16A4) &&
          Readable(session, 1) && !REX_LOAD_U8(session) &&
          !REX_LOAD_U8(player + 0x16A3) && !pdz_surface::IntroCameraActive() &&
          (REX_LOAD_U32(player + 4076) == weapon || REX_LOAD_U32(player + 4080) == weapon) &&
          (REX_LOAD_U32(action + 4) == 1 || REX_LOAD_U32(action + 4) == 12)) {
        inspect=true;
      }
    }
  }
  const bool wallRaised=UpdateWallRaise(ctx,base,action);
  // Keep the hold request active between native updates.
  if(wallRaised)REX_STORE_U8(action+47,1);
  __imp__sub_82232D58(ctx, base);

  const int first_ready=ReadyAnimationId(base,action);
  if (inspect && (REX_LOAD_U32(action+4)==1 || REX_LOAD_U32(action+4)==12) && !REX_LOAD_U8(action+45)) {
    // Add the custom bolt motion to the four native random wait choices.
    auto random=ctx;
    sub_826F2B28(random,base);
    bool bolt=first_ready>=0 && random.f1.f64<.2;
    inspection_playback={};
    auto call=ctx;
    call.r3.u64=action; call.r4.u64=23; call.r5.u64=0; call.r6.u64=0;
    call.f1.f64=0;
    if (bolt) sub_82234558(call,base);
    else { call.r4.u64=4; sub_82232C70(call,base); }
    if (!call.r3.u8 && !bolt && first_ready>=0) {
      bolt=true;
      call=ctx; call.r3.u64=action; call.r4.u64=23;
      call.r5.u64=0; call.r6.u64=0; call.f1.f64=0;
      sub_82234558(call,base);
    }
    if (call.r3.u8) {
      REX_STORE_U32(action+4,12);
      REX_STORE_U32(action+12,0);
      const auto controller=REX_LOAD_U32(REX_LOAD_U32(action+28)+1096);
      if (bolt && Readable(controller,1553)) {
        inspection_playback={action,controller,0,0,REX_LOAD_U32(action+20)};
        // Begin at the held pose, not the original weapon-below-screen draw.
        REX_STORE_U32(controller+196,0);
        REX_STORE_U32(controller+880,0);
        auto pose=ctx; pose.r3.u64=controller; pose.f1.f64=0;
        sub_8222FEB8(pose,base);
      }
      REXLOG_INFO("Weapon inspection: custom bolt {} animation {}",bolt,REX_LOAD_U32(action+20));
    }
  }
  if (Rules(base) == 0 || !REXCVAR_GET(pdz_campaign_zoom_auto) ||
      !LocalWeaponAction(base, action) || !Readable(action, 72)) return;

  // TU0 DW-P5 (MP5) and SuperDragon primary action classes.
  // FAC-16 uses its native held-trigger loop with the zoom lockout above removed.
  // Wait for the native shot/animation to finish; never interrupt reloads,
  // switching, alternate fire, or an already-running automatic burst.
  const auto type = REX_LOAD_U32(action);
  if (type != 0x8209A988 && type != 0x8209B538) return;
  const auto state = REX_LOAD_U32(action + 4);
  if ((state != 1 && state != 12) || !REX_LOAD_U8(action + 45)) return;
  const auto weapon = REX_LOAD_U32(action + 28);
  if (!Readable(weapon, 1320) || !REX_LOAD_U16(weapon + 1316)) return;
  const auto definition = REX_LOAD_U32(weapon + 860);
  if (!Readable(definition, 196)) return;
  const float interval = std::bit_cast<float>(REX_LOAD_U32(definition + 192));
  const float elapsed = std::bit_cast<float>(REX_LOAD_U32(action + 16));
  if (!std::isfinite(interval) || interval <= 0 || !std::isfinite(elapsed) || elapsed < interval) return;

  auto call = ctx;
  call.r3.u64 = action;
  sub_82231B98(call, base); // Native owner's aim query, not dual-wield count.
  if (!call.r3.u8) return;
  call = ctx;
  call.r3.u64 = action;
  if (type == 0x8209B538) sub_8224F018(call, base);
  else sub_82232200(call, base); // Native weapon readiness and timing gates.
  if (!call.r3.u8) return;
  call = ctx;
  call.r3.u64 = action;
  sub_822341C8(call, base); // Native primary shot start; preserves ammo/events.
}

REX_EXTERN(__imp__sub_8270D6C0);
REX_HOOK_RAW(sub_8270D6C0) {
  if (inspection_sampling && (ctx.r3.u32==inspection_sampling+48 ||
      ctx.r3.u32==inspection_sampling+732)) {
    const float progress=std::clamp(float(ctx.f1.f64),0.f,1.f);
    if (ctx.r3.u32==inspection_sampling+48) inspection_sample_progress=progress;
    // Use only the held-weapon portion: reverse toward the bolt, then return.
    const float travel=progress<.5f ? progress*2.f : (1.f-progress)*2.f;
    ctx.f1.f64=.999f-.599f*travel;
  }
  __imp__sub_8270D6C0(ctx,base);
}

REX_EXTERN(__imp__sub_8222FEB8);
REX_HOOK_RAW(sub_8222FEB8) {
  const auto controller=ctx.r3.u32;
  const auto action=inspection_playback.action;
  if (!action || controller!=inspection_playback.controller) {
    __imp__sub_8222FEB8(ctx,base); return;
  }
  if (!Readable(action,32) || !Readable(controller,1553) ||
      REX_LOAD_U32(action+4)!=12 || REX_LOAD_U32(action+20)!=inspection_playback.animation ||
      !LocalWeaponAction(base,action)) {
    inspection_playback={};
    __imp__sub_8222FEB8(ctx,base); return;
  }
  inspection_sampling=controller;
  inspection_sample_progress=inspection_playback.progress;
  __imp__sub_8222FEB8(ctx,base);
  inspection_sampling=0;
  inspection_playback.progress=inspection_sample_progress;
  // Keep the action clock monotonic; only the sampled arm/gun pose reverses.
  // Native completion and interruption logic therefore remain in control.
  REX_STORE_U32(controller+196,std::bit_cast<uint32_t>(inspection_sample_progress));
  REX_STORE_U32(controller+880,std::bit_cast<uint32_t>(inspection_sample_progress));
  if (inspection_sample_progress>=1.f) inspection_playback={};
}




// Native player visual-effect update: state 4 sets the cloaked bit (+64, 0x4).
// Override only for this call, preserving the weapon's own effect state so
// disabling the cheat follows the native uncloaking path on the next update.
REX_EXTERN(__imp__sub_82597D18);
REX_HOOK_RAW(sub_82597D18) {
  const uint32_t player=ctx.r3.u32;
  const uint32_t camera=pdz_input::Supported()?REX_LOAD_U32(0x82D68320):0;
  const bool apply=REXCVAR_GET(pdz_invisibility)&&Readable(camera,12)&&
      REX_LOAD_U32(camera+8)==player&&Readable(player,7924)&&REX_LOAD_U32(player+12)==2;
  if(!apply){__imp__sub_82597D18(ctx,base);return;}
  const uint32_t nativeEffect=REX_LOAD_U32(player+7904);
  const uint8_t nativeNotify=REX_LOAD_U8(player+7912);
  REX_STORE_U32(player+7904,4);
  REX_STORE_U8(player+7912,0);
  __imp__sub_82597D18(ctx,base);
  REX_STORE_U32(player+7904,nativeEffect);
  REX_STORE_U8(player+7912,nativeNotify);
}

// Resolve the reload playback rate once, before both hand and gun tracks are
// started. Native animation events still transfer ammo and finish the reload.
REX_EXTERN(__imp__sub_82234C08);
REX_HOOK_RAW(sub_82234C08) {
  const auto action=ctx.r3.u32,animation=ctx.r4.u32;
  const auto gun_out=ctx.r5.u32,speed_out=ctx.r7.u32;
  const auto before=ctx;
  __imp__sub_82234C08(ctx,base);
  const auto rules=Rules(base);
  if(!rules||(animation!=6&&animation!=16)||!LocalWeaponAction(base,action)||
     !Readable(gun_out,4)||!Readable(speed_out,4))return;
  const auto weapon=REX_LOAD_U32(action+28);
  if(!Readable(weapon,1318))return;
  const auto definition=REX_LOAD_U32(weapon+860),resource=REX_LOAD_U32(gun_out);
  if(!Readable(definition,1148)||!Readable(resource,12)||before.r1.u32<1024||
     !Readable(before.r1.u32-1024,1024))return;
  auto call=before;call.r1.u32-=1024;const auto descriptor=call.r1.u32+128;
  call.r3.u64=descriptor;call.r4.u64=REX_LOAD_U32(resource+8);
  sub_8270D2C8(call,base);
  const auto clip=REX_LOAD_U32(descriptor);
  if(!Readable(clip,32))return;
  const auto native=std::bit_cast<float>(REX_LOAD_U32(speed_out));
  const auto duration=std::bit_cast<float>(REX_LOAD_U32(clip+28));
  const auto id=REX_LOAD_U32(definition+1144);
  const float speed=float(pdz_reload::Speed(rules,id,REX_LOAD_U16(weapon+1316)==0,duration,native));
  REX_STORE_U32(speed_out,std::bit_cast<uint32_t>(speed));
}
