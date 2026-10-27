#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <atomic>
#include <array>
#include <mutex>
#include <unordered_map>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include "mp_characters.h"
#include "surface_runtime.h"
#include "blood_decals.h"

namespace {
thread_local uint32_t currentModelGraph = 0;
// Keep diagnostics useful after switching characters in the same session.
// A repeatedly failing material must not consume the entire logging budget.
bool FirstInvalidShaderDraw(uint32_t graph) {
  thread_local std::array<uint32_t, 64> reported{};
  thread_local unsigned count = 0;
  for (unsigned i = 0; i < count; ++i) if (reported[i] == graph) return false;
  if (count == reported.size()) return false;
  reported[count++] = graph;
  return true;
}
}

REX_EXTERN(__imp__sub_82710CC0);
REX_HOOK_RAW(sub_82710CC0) {
  // The command interpreter uses r6 as the base of per-instance model data.
  // Log 018 / dump 22612 faulted in its relative-data command: offset 0x1c
  // plus a null base, followed by lwz +4 (guest address 0x20). Do not execute
  // a draw with absent instance data; never substitute a different model's data.
  if (ctx.r6.u32 == 0) {
    static std::atomic<unsigned> skipped{0};
    if (skipped.fetch_add(1, std::memory_order_relaxed) < 16) {
      REXLOG_WARN("Skipping model command list with null instance data: graph={:08X} pass={:08X} caller={:08X}",
                  ctx.r3.u32, ctx.r5.u32, static_cast<uint32_t>(ctx.lr));
    }
    return;
  }
  const auto previousGraph = currentModelGraph;
  currentModelGraph = ctx.r3.u32;
  __imp__sub_82710CC0(ctx, base);
  currentModelGraph = previousGraph;
}

REX_EXTERN(__imp__sub_826EA128);
namespace {
std::mutex shaderOverflowMutex;
struct OverflowAllocation {uint32_t bytes;bool blood;};
std::unordered_map<uint32_t,OverflowAllocation> shaderOverflow;
uint32_t shaderOverflowBytes = 0;
uint32_t bloodOverflowBytes = 0;
}
REX_EXTERN(__imp__sub_8239DE28);
REX_HOOK_RAW(sub_8239DE28) {
  const auto caller = static_cast<uint32_t>(ctx.lr);
  const auto bytes = ctx.r3.u32, flags = ctx.r4.u32;
  __imp__sub_8239DE28(ctx, base);
  // Preserve native allocation first. Only shader code and the explicitly
  // scoped custom textures may use this tracked physical overflow storage.
  const bool shader = caller == 0x826E4188 && flags == 0xA5800000 && bytes <= 65536;
  const bool atlas = pdz_surface::AllocatingCabinAtlas() &&
      caller == 0x826ECE08 && flags == 0xBC800000 && bytes <= 4 * 1024 * 1024;
  const bool blood = pdz_blood::AllocatingTexture() &&
      caller == 0x826ECE08 && flags == 0xBC800000 && bytes <= 256 * 1024;
  if (ctx.r3.u32 || !bytes) return;
  if (!shader && !atlas && !blood) {
    if(pdz_blood::AllocatingTexture()) {
      static unsigned failures=0;
      if(failures++<4)REXLOG_ERROR("Blood native allocation failed outside pixel fallback: caller={:08X} flags={:08X} bytes={}",caller,flags,bytes);
    }
    return;
  }
  auto* runtime = rex::Runtime::instance();
  if (!runtime) return;
  const uint32_t committed = (bytes + 4095) & ~4095u;
  std::lock_guard lock(shaderOverflowMutex);
  // A separate 1 MiB allowance prevents cap textures from consuming the
  // shader/atlas reserve. The four shared textures need at most 592 KiB.
  if (blood ? bloodOverflowBytes + committed > 1024 * 1024 :
              shaderOverflowBytes + committed > 8 * 1024 * 1024) return;
  const auto address = runtime->memory()->SystemHeapAlloc(
      bytes,4096,rex::memory::kSystemHeapPhysical);
  if (!address) return;
  shaderOverflow.emplace(address,OverflowAllocation{committed,blood});
  (blood?bloodOverflowBytes:shaderOverflowBytes) += committed;
  ctx.r3.u64 = address;
  static unsigned reports = 0;
  if (reports++ < 8)
    REXLOG_INFO("Graphics overflow allocation: kind={} bytes={} address={:08X}",
                blood ? "blood/cap texture" : atlas ? "Surface atlas" : "vertex shader",bytes,address);
}
REX_EXTERN(__imp__sub_8239DFD8);
REX_HOOK_RAW(sub_8239DFD8) {
  // Native shader destruction frees its code pointer through this function.
  // Route only our own pointers back to their matching physical allocator.
  {
    std::lock_guard lock(shaderOverflowMutex);
    const auto found = shaderOverflow.find(ctx.r3.u32);
    if (found != shaderOverflow.end()) {
      rex::Runtime::instance()->memory()->SystemHeapFree(found->first);
      (found->second.blood?bloodOverflowBytes:shaderOverflowBytes) -= found->second.bytes;
      shaderOverflow.erase(found);
      ctx.r3.u64 = 0;
      return;
    }
  }
  __imp__sub_8239DFD8(ctx,base);
}
REX_EXTERN(__imp__sub_826E4378);
REX_HOOK_RAW(sub_826E4378) {
  if (!ctx.r4.u32 && ctx.lr == 0x82710F48)
    pdz_mp::RecoverRingleaderHeadShader(ctx, base, currentModelGraph);
  __imp__sub_826E4378(ctx, base);
}

REX_HOOK_RAW(sub_826EA128) {
  // Indexed draws flush shader state before emitting geometry. A campaign
  // material may have no shader for an MP render pass (Zhang Li, dump 35280).
  // Skip the entire invalid draw, not just the shader flush: otherwise the GPU
  // would draw with stale state from an unrelated material. Keep dirty flags
  // intact so the next valid material is flushed normally.
  const auto device = ctx.r3.u32;
  if (device && REX_LOAD_U32(device + 12948) == 0) {
    if (FirstInvalidShaderDraw(currentModelGraph)) {
      REXLOG_WARN("Skipping indexed draw without vertex shader: graph={:08X} device={:08X} pixel={:08X} caller={:08X}",
                  currentModelGraph, device, REX_LOAD_U32(device + 12944), static_cast<uint32_t>(ctx.lr));
    }
    return;
  }
  __imp__sub_826EA128(ctx, base);
}

