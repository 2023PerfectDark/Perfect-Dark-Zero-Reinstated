#include "debug_position.h"
#include "debug_position_store.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "game_cvars.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <memory>

namespace pdz_position {
namespace {
using Clock=std::chrono::steady_clock;
std::mutex mutex;
std::condition_variable wake;
std::unique_ptr<Store> store;
std::thread writer;
Bookmark saved;
bool stopping=false,dirty=false,pending=false,settled=false;
bool reached_checkpoint=false;
Clock::time_point ready_since{};
void WriteLoop(){
  std::unique_lock lock(mutex);
  for(;;){
    wake.wait_for(lock,std::chrono::milliseconds(500),[]{return stopping;});
    if(dirty){const auto next=saved;dirty=false;lock.unlock();const bool ok=store->Save(next);lock.lock();
      if(!ok){dirty=true;REXLOG_ERROR("Debug position: could not write the position bookmark");}}
    if(stopping)return;
  }
}
bool Access(uint32_t p,uint32_t n){
  auto* rt=rex::Runtime::instance();if(!rt||!p||uint64_t(p)+n>0x100000000ull)return false;
  for(uint64_t a=p;a<uint64_t(p)+n;){auto* h=rt->memory()->LookupHeap(uint32_t(a));uint32_t prot=0;
    if(!h||!h->QueryProtect(uint32_t(a),&prot)||!(prot&rex::memory::kMemoryProtectRead))return false;
    a=(a/h->page_size()+1)*h->page_size();}return true;
}
float Float(uint8_t* base,uint32_t p){return std::bit_cast<float>(REX_LOAD_U32(p));}
}
void Shutdown(){
  {std::lock_guard lock(mutex);stopping=true;}wake.notify_all();if(writer.joinable())writer.join();
}
void Configure(const std::filesystem::path& root){
  Shutdown();std::lock_guard lock(mutex);store=std::make_unique<Store>(root);saved={};
  store->Load(saved);pending=saved.enabled&&saved.present;dirty=false;stopping=false;settled=false;
  writer=std::thread(WriteLoop);
}
bool Enabled(){std::lock_guard lock(mutex);return saved.enabled;}
void Toggle(){std::lock_guard lock(mutex);saved.enabled=!saved.enabled;dirty=true;
  // Toggling during a run records the current location; it never teleports mid-mission.
  pending=false;REXLOG_INFO("Debug position: {}",saved.enabled?"enabled":"disabled");}
void Clear(){std::lock_guard lock(mutex);saved.present=false;saved.checkpoint=false;pending=false;dirty=true;}
void ResetLevel(){std::lock_guard lock(mutex);settled=false;reached_checkpoint=false;ready_since={};}
void CheckpointReached(uint8_t* base){
  if(!pdz_input::Supported()||!(REX_LOAD_U8(0x82E41930)&0x40))return;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(!Access(session,40)||REX_LOAD_U32(session+36)!=0||REX_LOAD_U8(session+25)>=14)return;
  Bookmark current{true,true,REX_LOAD_U8(session+25),REX_LOAD_U32(session+4),uint32_t(REXCVAR_GET(pdz_campaign_rules))};
  std::lock_guard lock(mutex);reached_checkpoint=true;
  // A checkpoint can trigger during a camera lock. Persist its arrival without
  // waiting for movement or a new unlocked player frame.
  if(store&&!stopping&&saved.enabled&&Matches(saved,current)&&!saved.checkpoint){saved.checkpoint=true;dirty=true;}
}
bool PrepareNativeCheckpoint(uint8_t* base){
  if(!pdz_input::Supported())return false;
  const auto session=REX_LOAD_U32(0x82D6B2B0);if(!Access(session,40))return false;
  Bookmark current{true,true,REX_LOAD_U8(session+25),REX_LOAD_U32(session+4),uint32_t(REXCVAR_GET(pdz_campaign_rules))};
  std::lock_guard lock(mutex);
  if(stopping||!store||!ResumeCheckpoint(saved,current,pending,REX_LOAD_U32(session+36)))return false;
  // sub_82270030 consumes this same selector for the native Checkpoint menu:
  // clears +24, sets level flag 0x20, then initializes the mission scripts.
  REX_STORE_U8(session+24,1);
  REXLOG_INFO("Debug position: requesting native checkpoint for mission {}",current.mission);
  return true;
}
void NativeCheckpointLoaded(){std::lock_guard lock(mutex);reached_checkpoint=true;}
void Tick(PPCContext& ctx,uint8_t* base,uint32_t player){
  if(!pdz_input::Supported()||!Access(player,6584))return;
  const auto local=REX_LOAD_U32(0x82E35468),session=REX_LOAD_U32(0x82D6B2B0);
  if(!Access(local,956)||REX_LOAD_U32(local+952)!=player||!Access(session,40))return;
  if(REX_LOAD_U32(session+36)!=0||REX_LOAD_U32(session+8)!=21)return;
  const auto physics=REX_LOAD_U32(player+228),health=REX_LOAD_U32(player+6560);
  if(!Access(physics,340)||!Access(health,976)||!(Float(base,health+968)>0)||
     REX_LOAD_U8(player+0x16A3)||pdz_surface::IntroCameraActive()||pdz_surface::EndingCameraActive())return;
  Bookmark current{true,true,REX_LOAD_U8(session+25),REX_LOAD_U32(session+4),uint32_t(REXCVAR_GET(pdz_campaign_rules))};
  for(unsigned i=0;i<3;++i)current.pose[i]=Float(base,player+48+i*4);
  current.pose[3]=Float(base,physics+336);current.pose[4]=Float(base,player+0x1674);
  if(!Valid(current))return;
  std::lock_guard lock(mutex);if(!store||!saved.enabled||stopping)return;
  // 0x40 is checkpoint reached; 0x20 is the native checkpoint-start branch.
  // Keep it latched for this attempt even when a script consumes either flag.
  reached_checkpoint|=(REX_LOAD_U8(0x82E41930)&0x60)!=0;
  current.checkpoint=reached_checkpoint&&current.mission<14;
  if(!settled){
    if(ready_since==Clock::time_point{}){ready_since=Clock::now();return;}
    if(Clock::now()-ready_since<std::chrono::seconds(1))return;
    if(pending&&Matches(saved,current)){
      auto call=ctx;call.r1.u32-=512;if(!Access(call.r1.u32,512))return;
      const auto pos=call.r1.u32+128;
      for(unsigned i=0;i<3;++i)REX_STORE_U32(pos+i*4,std::bit_cast<uint32_t>(saved.pose[i]));
      call.r3.u64=player;call.r4.u64=pos;sub_8220EC58(call,base);
      REX_STORE_U32(physics+336,std::bit_cast<uint32_t>(saved.pose[3]));
      REX_STORE_U32(player+0x1674,std::bit_cast<uint32_t>(saved.pose[4]));
      REXLOG_INFO("Debug position restored: mission={} xyz=({},{},{})",saved.mission,saved.pose[0],saved.pose[1],saved.pose[2]);
      pending=false;settled=true;return; // Never replace the bookmark with the pre-teleport location.
    }
    pending=false;settled=true;
  }
  if(!Matches(saved,current)||saved.pose!=current.pose||saved.checkpoint!=current.checkpoint){saved=current;dirty=true;}
}
}

REX_EXTERN(__imp__sub_82270030);
REX_HOOK_RAW(sub_82270030){
  const bool resume=pdz_position::PrepareNativeCheckpoint(base);
  __imp__sub_82270030(ctx,base);
  if(resume)pdz_position::NativeCheckpointLoaded();
}
