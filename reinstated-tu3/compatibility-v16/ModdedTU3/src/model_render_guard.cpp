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
#include "model_sync_check.h"

namespace {
bool ModelMemory(uint32_t address,uint32_t bytes,bool write);
// Dump 64100 failed the native trailing allocation sentinel while freeing
// actor+1240, the nine-pose buffer owned by actor+1012. Observe its actual
// allocator bounds; do not repair sentinel bytes or suppress native checks.
void TraceAnimationBuffer(PPCContext call,uint8_t* base,uint32_t state,const char* phase) {
  if(!ModelMemory(state,2852,false))return;
  const auto buffer=REX_LOAD_U32(state+228);
  if(!buffer||buffer<28||!ModelMemory(buffer-28,28,false))return;
  const auto size=REX_LOAD_U32(buffer-12),raw=REX_LOAD_U32(buffer-16);
  if(!size||size>4*1024*1024||!ModelMemory(raw,4,false))return;
  call.r1.u32-=512;
  if(!ModelMemory(call.r1.u32,512,true))return;
  call.r3.u64=raw;sub_82742B58(call,base);
  const uint64_t end=uint64_t(raw)+call.r3.u32,used=uint64_t(buffer)+size;
  if(end<used||end-used>65536||end>0x100000000ull)return;
  if(end>used&&!ModelMemory(uint32_t(used),uint32_t(end-used),false))return;
  uint32_t bad=0;
  for(uint64_t p=used;p<end;++p)if(REX_LOAD_U8(uint32_t(p))!=255){bad=uint32_t(p);break;}
  const auto frame=REX_LOAD_U32(state+2840);
  uint32_t wanted=0;
  if(frame<2){const auto pose=state+2320+frame*228;
    wanted=(uint32_t(REX_LOAD_U16(pose+44))+12u*REX_LOAD_U16(pose+46))*4u*9u;}
  if(bad||wanted>size){
    static std::atomic<unsigned> reports{0};
    if(reports.fetch_add(1,std::memory_order_relaxed)<64)
      REXLOG_ERROR("Animation allocation trace: phase={} actor={:08X} state={:08X} buffer={:08X} capacity={} pose_bytes={} bad={:08X} byte={} animation={:08X}",
        phase,state-1012,state,buffer,size,wanted,bad,bad?unsigned(REX_LOAD_U8(bad)):0,REX_LOAD_U32(state+2088));
  }
}
thread_local uint32_t syncDefinition=0,syncSource=0,syncDestination=0;
bool ModelMemory(uint32_t address,uint32_t bytes,bool write) {
  auto* rt=rex::Runtime::instance();
  if(!rt||!address||uint64_t(address)+bytes>0x100000000ull)return false;
  for(uint64_t p=address;p<uint64_t(address)+bytes;) {
    auto* heap=rt->memory()->LookupHeap(uint32_t(p));
    uint32_t protection=0;
    const auto required=write?rex::memory::kMemoryProtectWrite:rex::memory::kMemoryProtectRead;
    if(!heap||!heap->QueryProtect(uint32_t(p),&protection)||!(protection&required))return false;
    p=(p/heap->page_size()+1)*heap->page_size();
  }
  return true;
}
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

REX_EXTERN(__imp__sub_8270B238);
REX_EXTERN(__imp__sub_82553398);
REX_HOOK_RAW(sub_82553398) {
  const auto state=ctx.r3.u32;
  bool changing=false;
  if(ModelMemory(state,2852,false)&&ModelMemory(ctx.r5.u32,24,false)) {
    const auto next=REX_LOAD_U32(ctx.r5.u32+20),previous=REX_LOAD_U32(state+2088);
    changing=next&&(!previous||(ModelMemory(previous,24,false)&&REX_LOAD_U32(previous+20)!=next));
  }
  if(changing)TraceAnimationBuffer(ctx,base,state,"before animation");
  __imp__sub_82553398(ctx,base);
  if(changing)TraceAnimationBuffer(ctx,base,state,"after animation");
}
REX_EXTERN(__imp__sub_82543C18);
REX_HOOK_RAW(sub_82543C18) {
  TraceAnimationBuffer(ctx,base,ctx.r3.u32+1012,"before character cleanup");
  __imp__sub_82543C18(ctx,base);
}
REX_HOOK_RAW(sub_8270B238) {
  // Dump 29320: the initial model-state memcpy (return 8270B264) wrote
  // through a null destination. Reject the incomplete sync as a whole.
  // Do not redirect it into another frame's storage or swallow memcpy faults.
  if(!pdz_model_sync::Ready(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,
      [&](uint32_t p){return REX_LOAD_U32(p);},ModelMemory)) {
    static std::atomic<unsigned> reports{0};
    if(reports.fetch_add(1,std::memory_order_relaxed)<16)
      REXLOG_WARN("Incomplete model sync rejected: definition={:08X} source={:08X} destination={:08X} caller={:08X}",
          ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,uint32_t(ctx.lr));
    return;
  }
  const auto previousDefinition=syncDefinition,previousSource=syncSource,previousDestination=syncDestination;
  syncDefinition=ctx.r3.u32;syncSource=ctx.r4.u32;syncDestination=ctx.r5.u32;
  __imp__sub_8270B238(ctx,base);
  syncDefinition=previousDefinition;syncSource=previousSource;syncDestination=previousDestination;
}

REX_EXTERN(__imp__sub_82714220);
REX_HOOK_RAW(sub_82714220) {
  // Dump 9256 failed in the table walk, before the material-copy safeguard.
  // Do not walk a missing table, manufacture pointers, or copy a partial node.
  if(!pdz_model_sync::NodeTablesReady(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,
       [&](uint32_t p){return REX_LOAD_U32(p);},ModelMemory)) {
    static std::atomic<unsigned> reports{0};
    if(reports.fetch_add(1,std::memory_order_relaxed)<16)
      REXLOG_ERROR("Invalid model node tables: node={:08X} source={:08X} destination={:08X} graph={:08X} frames={:08X}/{:08X}",
        ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,syncDefinition,syncSource,syncDestination);
    return;
  }
  __imp__sub_82714220(ctx,base);
}

REX_EXTERN(__imp__sub_827175C0);
REX_HOOK_RAW(sub_827175C0) {
  // Validate all native material copy regions before the first write. A valid
  // outer frame buffer does not imply valid nested matrix/effect storage.
  if(!pdz_model_sync::MaterialReady(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,
      [&](uint32_t p){return REX_LOAD_U32(p);},
      [&](uint32_t p){return REX_LOAD_U16(p);},
      [&](uint32_t p){return REX_LOAD_U8(p);},ModelMemory)) {
    static std::atomic<unsigned> reports{0};
    if(reports.fetch_add(1,std::memory_order_relaxed)<16) {
      REXLOG_WARN("Incomplete material sync rejected: material={:08X} source={:08X} destination={:08X} graph={:08X} frames={:08X}/{:08X}",
          ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,syncDefinition,syncSource,syncDestination);
      for(unsigned offset=0;offset<=16;offset+=4) {
        const auto src=ctx.r4.u32,dst=ctx.r5.u32;
        if(ModelMemory(src,20,false)&&ModelMemory(dst,20,false))
          REXLOG_WARN("Material buffer +{}: source={:08X} destination={:08X}",offset,REX_LOAD_U32(src+offset),REX_LOAD_U32(dst+offset));
      }
    }
    return;
  }
  __imp__sub_827175C0(ctx,base);
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

