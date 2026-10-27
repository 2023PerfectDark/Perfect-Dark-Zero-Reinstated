#include "generated/default/perfectdarkzerorecomp_init.h"
#include "mousehook.h"
#include "utils.h"
#include "cutscene_timing.h"
#include "surface_runtime.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <chrono>
#include <fstream>
#include <atomic>

REXCVAR_DEFINE_BOOL(pdz_cutscene_timing_fix,false,"Gameplay",
    "Experimental: prevent cutscene animation advancing ahead of its native clock; allow high FPS");
namespace {
std::atomic<bool> active{false};
}
namespace pdz_cutscene {
double AheadFallback(double original){
  return pdz_input::Supported()&&REXCVAR_GET(pdz_cutscene_timing_fix)?0.0:original;
}
bool UncapActive(){return REXCVAR_GET(pdz_cutscene_timing_fix)&&active.load();}
}

REXCVAR_DEFINE_BOOL(pdz_cutscene_timing_trace,false,"Debug",
    "Measure native cutscene clock/animation against wall time; no timing writes");

REX_EXTERN(__imp__sub_82217AD8);
REX_HOOK_RAW(sub_82217AD8){
  if(pdz_input::Supported())pdz_surface::HandleHostCutscene(ctx,base,ctx.r3.u32);
  if(pdz_input::Supported())active=REX_LOAD_U32(ctx.r3.u32+12)==3;
  if(pdz_input::Supported()&&REXCVAR_GET(pdz_cutscene_timing_trace)){
    const auto object=ctx.r3.u32;
    // This is the native cutscene update's own valid receiver.
    const auto state=REX_LOAD_U32(object+12);
    static thread_local uint32_t previous_object=0,previous_state=0;
    static thread_local unsigned frames=0;
    static thread_local std::chrono::steady_clock::time_point start{},report{};
    const auto now=std::chrono::steady_clock::now();
    const bool changed=object!=previous_object||state!=previous_state;
    if(changed){start=report=now;frames=0;previous_object=object;previous_state=state;}
    ++frames;
    const auto interval=std::chrono::duration<double>(now-report).count();
    if(changed||interval>=1.0){
      uint32_t clock=0,animation=0;
      if(state==3){
        auto call=ctx;call.r1.u32-=512;
        sub_8221A598(call,base);clock=call.r3.u32;
        call=ctx;call.r1.u32-=512;sub_8221A5E8(call,base);animation=call.r3.u32;
      }
      std::ofstream out(utils::RepoRoot()/"out/cutscene-timing.csv",std::ios::app);
      out<<object<<','<<state<<','<<std::chrono::duration<double>(now-start).count()
         <<','<<clock<<','<<animation<<','<<(interval>0?frames/interval:0)
         <<','<<ctx.f1.f64<<'\n';
      frames=0;report=now;
    }
  }
  __imp__sub_82217AD8(ctx,base);
}
