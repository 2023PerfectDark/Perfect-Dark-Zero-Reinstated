#include "game_cvars.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "mousehook.h"
#include "debug_checkpoint_trace.h"
#include "debug_position.h"
#include <fstream>
#include <mutex>
#include <array>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

// Opt-in research hooks. They do not write guest state or claim to save it.
REXCVAR_DEFINE_BOOL(pdz_checkpoint_trace, false, "Debug",
                    "Log native checkpoint transitions for checkpoint persistence development");
namespace {
bool Readable(uint32_t address, uint32_t length) {
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !address || uint64_t(address) + length > 0x100000000ull) return false;
  for (uint64_t p = address; p < uint64_t(address) + length;) {
    auto* heap = runtime->memory()->LookupHeap(uint32_t(p));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(p), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    p = (p / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
void Trace(const char* phase, uint8_t* base) {
  if (!REXCVAR_GET(pdz_checkpoint_trace) || !pdz_input::Supported()) return;
  const uint32_t session = REX_LOAD_U32(0x82D6B2B0);
  if (!Readable(session, 40)) return;
  // Keep diagnostics independent of runtime logger filtering and buffering.
  // Events only: no per-frame file writes and no state modification.
  static std::mutex output_mutex;
  const std::lock_guard lock(output_mutex);
  std::ofstream output("checkpoint-transitions.txt", std::ios::app);
  output << phase << " mission=" << unsigned(REX_LOAD_U8(session + 25))
         << " difficulty=" << REX_LOAD_U32(session + 4)
         << " mode=" << REX_LOAD_U32(session + 36)
         << " state=" << REX_LOAD_U32(session + 8)
         << " retry=" << unsigned(REX_LOAD_U8(session + 24))
         << " flags=" << unsigned(REX_LOAD_U8(0x82E41930)) << '\n';
  REXLOG_INFO("Checkpoint trace {}: mission={} difficulty={} mode={} session_state={} retry={} level_flags={:02X}",
      phase, REX_LOAD_U8(session + 25), REX_LOAD_U32(session + 4),
      REX_LOAD_U32(session + 36), REX_LOAD_U32(session + 8),
      REX_LOAD_U8(session + 24), REX_LOAD_U8(0x82E41930));
}
}
namespace pdz_checkpoint {
void Observe(uint8_t* base) {
  if (!REXCVAR_GET(pdz_checkpoint_trace) || !pdz_input::Supported()) return;
  const auto session = REX_LOAD_U32(0x82D6B2B0);
  if (!Readable(session, 40)) return;
  const std::array<uint32_t, 5> current{session, REX_LOAD_U8(session + 25),
      REX_LOAD_U32(session + 8), REX_LOAD_U8(session + 24),
      REX_LOAD_U8(0x82E41930)};
  static thread_local std::array<uint32_t, 5> previous{};
  if (current != previous) {
    previous = current;
    Trace("observed player-update transition", base);
  }
}
}
REX_EXTERN(__imp__sub_82142C50);
REX_HOOK_RAW(sub_82142C50) {
  const bool checkpoint_bit = ctx.r4.u32 == 6;
  __imp__sub_82142C50(ctx, base);
  if (checkpoint_bit) {
    pdz_position::CheckpointReached(base);
    Trace("checkpoint flag set", base);
  }
}
REX_EXTERN(__imp__sub_8226FEA0);
REX_HOOK_RAW(sub_8226FEA0) {
  Trace("before mission reconstruction", base);
  __imp__sub_8226FEA0(ctx, base);
  Trace("after mission reconstruction", base);
}
REX_EXTERN(__imp__sub_82270548);
REX_HOOK_RAW(sub_82270548) {
  Trace("before mission activation", base);
  __imp__sub_82270548(ctx, base);
  Trace("after mission activation", base);
}
