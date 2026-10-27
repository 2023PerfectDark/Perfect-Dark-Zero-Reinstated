// TU3's main arena is too small for the existing 1,024-character pool.
// Enlarge its native backing before construction, retaining native ownership,
// allocation/free bookkeeping, character stride, and registry sentinels.
#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <chrono>

bool Tu3CrowdHasModelBudget(uint8_t* base) {
  // sub_82189918 scans 170 collision records (80 bytes each). On exhaustion
  // it uses index -1 and writes before the table, corrupting later collision
  // iteration. Stop optional actors before entering that native constructor.
  // Keep the whole native registration/removal system and authored NPCs intact.
  constexpr uint32_t collisionStates=0x82E5D590, collisionCapacity=170;
  constexpr uint32_t collisionReserve=64;
  uint32_t collisionAvailable=0;
  for(uint32_t i=0;i<collisionCapacity;++i)
    if(!(REX_LOAD_U32(collisionStates+i*80+68)&1))++collisionAvailable;
  // Audited against TU3 sub_82410A60: 2,500 model slots, state 4 is free.
  // Increasing actor storage does not increase this separately indexed pool.
  // Check before the actor factory: its constructor cannot unwind a missing
  // body model. Keep spare slots for native mission events and effects.
  constexpr uint32_t states=0x82D70E80, capacity=2500, reserve=128;
  uint32_t available=0;
  for(uint32_t i=0;i<capacity;++i)
    if(REX_LOAD_U32(states+i*4)==4)++available;
  if(available>reserve && collisionAvailable>collisionReserve)return true;
  static auto next=std::chrono::steady_clock::time_point{};
  const auto now=std::chrono::steady_clock::now();
  if(now>=next) {
    next=now+std::chrono::seconds(5);
    REXLOG_INFO("TU3 crowd deferred: models {}/{} free/reserved, collisions {}/{} free/reserved",
      available,reserve,collisionAvailable,collisionReserve);
  }
  return false;
}

REX_EXTERN(__imp__sub_82189918);
REX_HOOK_RAW(sub_82189918) {
  // The final stack argument is a requested collision index, or -1 to find
  // one. Native destruction/update already recognizes actor+852 == -1 as
  // unregistered. On exhaustion preserve that state instead of indexing -1
  // into the global table (which also overwrites the nearby player reference).
  const auto requested=REX_LOAD_U32(ctx.r1.u32+100);
  if(requested==0xffffffffu) {
    bool free=false;
    for(uint32_t i=0;i<170;++i)
      if(!(REX_LOAD_U32(0x82E5D590+i*80+68)&1)){free=true;break;}
    if(!free) {
      REX_STORE_U32(ctx.r3.u32+852,0xffffffffu);
      static auto next=std::chrono::steady_clock::time_point{};
      const auto now=std::chrono::steady_clock::now();
      if(now>=next) {
        next=now+std::chrono::seconds(5);
        REXLOG_WARN("TU3 collision table full: prevented out-of-bounds registration for actor {:08X}",ctx.r3.u32);
      }
      return;
    }
  }
  __imp__sub_82189918(ctx,base);
}

REX_EXTERN(__imp__sub_8239DB88);
REX_HOOK_RAW(sub_8239DB88) {
  constexpr uint32_t config = 0x82D28458 + 3 * 48;
  constexpr uint32_t original = 0x01280000;
  if (ctx.r3.u32 == 3 && REX_LOAD_U32(config + 8) == 1 &&
      REX_LOAD_U32(config + 16) == original &&
      REX_LOAD_U32(config + 24) == 0) {
    REX_STORE_U32(config + 16, original + 0x01000000);
    REXLOG_INFO("TU3 character-pool backing: native main arena expanded by 16 MiB");
  }
  __imp__sub_8239DB88(ctx, base);
}
