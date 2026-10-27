#include "generated/default/pdzstocktu3_init.h"
#include "mousehook.h"
#include "shader_loading.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

namespace {
bool Readable(uint32_t p, uint32_t length) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !p || uint64_t(p) + length > 0x100000000ull) return false;
  for (uint64_t a = p; a < uint64_t(p) + length;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(a));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(a), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    a = (a / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
bool Session(uint8_t* base, uint32_t& session) {
  session = REX_LOAD_U32(0x82D6BFD8);
  return Readable(session, 44);
}
}

REX_EXTERN(__imp__sub_822788D8);
REX_HOOK_RAW(sub_822788D8) {
  // Native level loader selects the MP registry for mode 2, and the campaign
  // registry otherwise. Record every invocation, including reloads and DLC.
  uint32_t session;
  if (pdz_input::Supported() && Session(base, session)) {
    const auto mode = REX_LOAD_U32(session + 40);
    const auto id = REX_LOAD_U8(session + 25);
    char name[160]{};
    if (ctx.r1.u32 >= 512 && Readable(ctx.r1.u32 - 512, 512)) {
      auto call = ctx;
      call.r1.u32 -= 512;
      call.r3.u64 = id;
      if (mode == 2) sub_822774D0(call, base);
      else sub_82277438(call, base);
      const auto address = call.r3.u32;
      if (Readable(address, sizeof(name)))
        for (size_t i = 0; i + 1 < sizeof(name); ++i) {
          name[i] = char(REX_LOAD_U8(address + i));
          if (!name[i]) break;
        }
    }
    pdz_shaders::SetMap(int(mode), int(id), name);
  }
  __imp__sub_822788D8(ctx, base);
}

REX_EXTERN(__imp__sub_822301A0);
REX_HOOK_RAW(sub_822301A0) {
  __imp__sub_822301A0(ctx, base);
  if (!pdz_input::Supported() || !(ctx.r3.u32 & 255)) return;
  uint32_t session;
  if (!Session(base, session)) return;
  // Audited in sub_8222D4A8: states 20 (load) and 18 (restart) test this
  // completion predicate immediately before switching the loading scene out.
  // Other callers include unload/menu transitions and must remain unchanged.
  const auto state = REX_LOAD_U32(session + 8);
  if ((state == 20 || state == 18) && pdz_shaders::HoldLoadingScreen())
    ctx.r3.u64 = 0;
}
