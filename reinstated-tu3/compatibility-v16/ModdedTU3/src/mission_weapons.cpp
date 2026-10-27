#include "generated/default/pdzstocktu3_init.h"
#include "mission_weapon_policy.h"
#include "mousehook.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

namespace {
bool Writable(uint32_t address, unsigned size) {
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !address || uint64_t(address) + size > 0x100000000ull) return false;
  for (uint64_t at = address; at < uint64_t(address) + size;) {
    auto* heap = runtime->memory()->LookupHeap(uint32_t(at));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(at), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead) ||
        !(protection & rex::memory::kMemoryProtectWrite)) return false;
    at = (at / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
struct RestoreLoadout {
  uint8_t* base;
  uint32_t address;
  pdz_mission_weapons::Loadout original;
  ~RestoreLoadout() {
    for (unsigned i = 0; i < original.size(); ++i)
      REX_STORE_U32(address + i * 4, original[i]);
  }
};
}

// This native routine copies profile+8028 into the save buffer and computes
// its CRC before queueing the write. Filter BEFORE that copy, and restore the
// live profile afterward. Changing the completed buffer would invalidate CRC.
REX_EXTERN(__imp__sub_8250DC10);
REX_HOOK_RAW(sub_8250DC10) {
  const auto profile = ctx.r3.u32;
  if (!pdz_input::Supported() || uint64_t(profile) + 9460 > 0x100000000ull ||
      !Writable(profile + 9444, 16)) {
    __imp__sub_8250DC10(ctx, base);
    return;
  }
  pdz_mission_weapons::Loadout live{};
  for (unsigned i = 0; i < live.size(); ++i) live[i] = REX_LOAD_U32(profile + 9444 + i * 4);
  const auto saved = pdz_mission_weapons::PersistentLoadout(live);
  if (saved == live) {
    __imp__sub_8250DC10(ctx, base);
    return;
  }
  const RestoreLoadout restore{base, profile + 9444, live};
  for (unsigned i = 0; i < saved.size(); ++i) REX_STORE_U32(profile + 9444 + i * 4, saved[i]);
  __imp__sub_8250DC10(ctx, base);
}

// This function serves both the live inventory UI and the persistent loadout
// editor. Leave live grants alone; prevent the menu branch inserting the Klobb.
REX_EXTERN(__imp__sub_823B4C10);
REX_HOOK_RAW(sub_823B4C10) {
  if (pdz_input::Supported() && pdz_mission_weapons::MissionOnly(ctx.r4.u32) &&
      REX_LOAD_U32(0x82E2F66C) == 0) return;
  __imp__sub_823B4C10(ctx, base);
}
