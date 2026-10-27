#include "generated/default/perfectdarkzerorecomp_init.h"
#include "mousehook.h"
#include "utils.h"
#include "surface_runtime.h"
#include "disco_dwell.h"
#include "weapon_rules.h"
#include "klobb_weapon.h"
#include "surface_mission.h"
#include "surface_ending.h"
#include "surface_intro_keys.h"
#include "surface_intro_animation_data.h"
#include <chrono>
#include <atomic>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <array>
#include "surface_camera_ground.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <SDL3/SDL.h>
#include "nightclub_population.h"
#include "mission_weather.h"
#include <algorithm>

REXCVAR_DEFINE_BOOL(pdz_surface_test, true, "Debug", "Enable the custom Surface terrain");
REXCVAR_DEFINE_BOOL(pdz_surface_collision_test, true, "Debug", "Use Surface collision in the custom mission");
REXCVAR_DEFINE_BOOL(pdz_surface_objectives_test, true, "Debug", "Enable Surface objectives in the native mission menu");
REXCVAR_DEFINE_BOOL(pdz_surface_mission_slot, true, "Debug", "Register Surface in native solo mission slot 14");
REXCVAR_DEFINE_INT32(pdz_surface_host_mission, 4, "Debug", "Surface host mission for environment migration tests; 4 uses Mansion Infiltration").range(0,13);
REXCVAR_DEFINE_BOOL(pdz_surface_snow_material_test, true, "Audio", "Use native snow walking and running sounds on Surface terrain");
REXCVAR_DEFINE_BOOL(pdz_surface_daytime, false, "Game", "Daytime sky and cold daylight fog for the custom Surface mission");
REXCVAR_DEFINE_BOOL(pdz_surface_intro_test, true, "Debug", "Preview GoldenEye Surface opening camera using native PDZ rendering");
REXCVAR_DEFINE_DOUBLE(pdz_surface_intro_preview_speed, 1.0, "Debug", "Surface fly-in playback speed for animation inspection");
REXCVAR_DEFINE_BOOL(pdz_surface_intro_pose_inspection, false, "Debug", "Loop the intro pose with a close inspection camera");
REXCVAR_DEFINE_BOOL(pdz_surface_debug_unlock, false, "Debug", "Expose Surface for isolated development testing without changing profile completion");
REXCVAR_DEFINE_BOOL(pdz_unlock_all_missions, true, "Gameplay", "Expose all registered campaign missions without changing completion records");
REXCVAR_DEFINE_DOUBLE(pdz_surface_music_volume, 0.45, "Audio", "Surface mission soundtrack volume (0 to 1)");
REXCVAR_DEFINE_DOUBLE(pdz_disco_music_volume, 0.90, "Audio", "Nightclub disco Easter egg volume (0 to 1)");

namespace {
thread_local bool allocating_cabin_atlas=false;
void Trace(const std::string& message){
  std::ofstream log(utils::RepoRoot()/"out/surface/runtime.log",std::ios::app);
  log<<message<<'\n';
}
uint32_t test_background=0,host_background=0,spawned_player=0;
uint32_t surface_objectives=0,objective_records=0;
uint32_t safe_key_draw_count=0,safe_key_first_triangle=0;
std::atomic<float> objective_notification_seconds{0.f};
std::unordered_set<uint32_t> surface_observed_entities;
struct CabinGuard {
  uint32_t actor=0;
  bool positioned=false;
};
std::array<CabinGuard,7> cabin_guards{};
struct CabinKeyVertex {uint32_t address;std::array<float,3> position;};
std::vector<CabinKeyVertex> cabin_key_vertices;
std::vector<uint32_t> klobb_pickup_vertices;
bool klobb_pickup_hidden=false;

bool ladder_spawn_attempted=false;
bool barrel_spawn_attempted=false;
std::vector<uint32_t> surface_prop_setups;
unsigned cabin_dialogue_line=0;
double cabin_dialogue_seconds=0;
std::chrono::steady_clock::time_point cabin_dialogue_last{};
bool surface_registered=false;
pdz_surface::Mission surface_mission;
constexpr uint32_t kSurfaceTextBase=0x7F535500;
constexpr std::array<const char*,5> objective_instructions{
  "Find the satellite control building and switch off the communications dish.",
  "Enter the guard cabin and collect the safe key from the table.",
  "Unlock the safe and recover the building plans.",
  "Use the Audioscope to record the officers' conversation at the helipad.",
  "Destroy the four ventilation locks and enter the base after completing your objectives."
};
std::array<float,3> spawn{};
std::vector<pdz_surface::CameraGroundTriangle> camera_ground;
// Captured from the user's chosen Surface starting spot, 2026-10-04.
constexpr std::array<float,3> kSurfaceStart{-21390.376953125f,72.5935287475586f,-51186.8203125f};
constexpr float kSurfaceStartYaw=-1.523909091949463f;
constexpr float kSurfaceStartPitch=-.6900513172149658f;
std::vector<uint32_t> surface_spiderbots;
std::vector<uint32_t> surface_snow_emitters;
thread_local bool surface_datacore_initializing=false;
struct IntroPreview {
  uint32_t player=0;
  double seconds=0;
  std::chrono::steady_clock::time_point last{};
  bool active=false;
  uint8_t previous_lock=0;
  bool started=false;
  std::array<float,3> anchor{};
  float yaw=kSurfaceStartYaw;
} intro;
uint32_t intro_animation_model=0;
bool intro_animation_attempted=false;
constexpr double kIntroFlyStart=8.15;
constexpr double kIntroApproachSeconds=6.5;
constexpr double kIntroAimHoldSeconds=0.0;
constexpr double kIntroCameraMergeSeconds=.65;
constexpr double kIntroApproachTicks=236.0; // Close shot, before the eye-position keys.
std::atomic<float> intro_pose_frame{-1.f};
thread_local bool evaluating_intro_body=false;
struct EndingPreview {
  pdz_surface::VentEntry entry;
  uint32_t player=0;
  double seconds=0;
  bool active=false, dispatched=false;
  uint8_t previous_lock=0;
  std::chrono::steady_clock::time_point last{};
} ending;
std::atomic<bool> intro_replay_requested{false};
std::atomic<bool> intro_title_skip_requested{false};
std::atomic<float> intro_fade_opacity{0.f};
std::atomic<bool> intro_presentation_active{false};
std::atomic<int> intro_caption{0};
bool host_intro_skipped=false;
struct MissionMusic {
  const char* path="out/surface/mission-music.wav";
  bool disco=false;
  SDL_AudioStream* stream=nullptr;
  std::ifstream file;
  uint32_t size=0,remaining=0,rate=0;
  uint16_t channels=0;
  bool attempted=false,audio_initialized=false;
  void Stop(){
    if(stream)SDL_DestroyAudioStream(stream);
    if(audio_initialized)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audio_initialized=false;
    stream=nullptr;file.close();file.clear();attempted=false;
  }
  void Pause(){if(stream)SDL_PauseAudioStreamDevice(stream);}
  void Tick(){
    if(!attempted){
      attempted=true;
      file.open(utils::RepoRoot()/path,std::ios::binary);
      std::array<uint8_t,44> h{};file.read(reinterpret_cast<char*>(h.data()),h.size());
      auto u16=[&](size_t o){return uint16_t(h[o]|uint16_t(h[o+1])<<8);};
      auto u32=[&](size_t o){return uint32_t(u16(o))|uint32_t(u16(o+2))<<16;};
      if(!file||std::memcmp(h.data(),"RIFF",4)||std::memcmp(h.data()+8,"WAVEfmt ",8)||
         u32(16)!=16||u16(20)!=1||u16(34)!=16||std::memcmp(h.data()+36,"data",4))return;
      channels=u16(22);rate=u32(24);size=remaining=u32(40);
      if(!channels||channels>8||rate<8000||rate>192000||!size||size%(channels*2))return;
      SDL_AudioSpec spec{SDL_AUDIO_S16LE,int(channels),int(rate)};
      if(!SDL_InitSubSystem(SDL_INIT_AUDIO))return;
      audio_initialized=true;
      stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
      if(!stream)return;
      Trace("Surface mission soundtrack started");
    }
    if(!stream)return;
    const double volume=disco?REXCVAR_GET(pdz_disco_music_volume):REXCVAR_GET(pdz_surface_music_volume);
    SDL_SetAudioStreamGain(stream,float(std::clamp(volume,0.0,1.0)));
    std::array<char,32768> chunk;
    // Keep about half a second queued; never load the extended track into RAM.
    while(SDL_GetAudioStreamQueued(stream)<int(rate*channels)){
      if(!remaining){file.clear();file.seekg(44);remaining=size;}
      auto n=std::min<uint32_t>(remaining,uint32_t(chunk.size()));n-=n%(channels*2);
      file.read(chunk.data(),n);
      if(!file||!SDL_PutAudioStreamData(stream,chunk.data(),int(n))){
        SDL_DestroyAudioStream(stream);stream=nullptr;Trace("Surface soundtrack read/playback failed");return;
      }
      remaining-=n;
    }
    SDL_ResumeAudioStreamDevice(stream);
  }
} music;
bool native_music_muted=false;
// These are the categories selected by PDZ's own music-volume calculation
// (821F3E00). All remaining categories use the effects-volume preference.
bool MusicCategory(uint32_t category){
  return category==2 || category==15 || category==20 || category==21;
}
void SetNativeMusicMuted(PPCContext& ctx,uint8_t* base,bool muted){
  if(native_music_muted==muted)return;
  native_music_muted=muted;
  for(uint32_t category:{2u,15u,20u,21u}){
    auto call=ctx;call.r1.u32-=512;
    call.r3.u64=category;
    // Preserve the user's preference and the native per-category fade level.
    call.r4.u64=REX_LOAD_U8(0x82E66E80+category*32+28);
    sub_821F3E00(call,base);
    call.r4.u64=muted?0:call.r3.u32;
    call.r3.u64=category;
    sub_8274E018(call,base);
  }
  Trace(muted?"Native mission music volume suppressed":"Native music volume restored");
}
bool Readable(uint32_t p, uint32_t n) {
  auto* memory=rex::Runtime::instance()->memory();
  if (!p || !n || uint64_t(p)+n>0x100000000ull) return false;
  for(uint64_t at=p;at<uint64_t(p)+n;){
    auto* heap=memory->LookupHeap(uint32_t(at)); uint32_t protection=0;
    if(!heap || !heap->QueryProtect(uint32_t(at),&protection) ||
       !(protection&rex::memory::kMemoryProtectRead))return false;
    at=(at/heap->page_size()+1)*heap->page_size();
  }
  return true;
}
#include "disco_music.inl"
bool SurfaceObjective(uint32_t object){
  return surface_objectives && object>=surface_objectives &&
      object<surface_objectives+5*52 && (object-surface_objectives)%52==0;
}
bool SurfaceRequested(uint8_t* base){
  if(!REXCVAR_GET(pdz_surface_test))return false;
  if(!REXCVAR_GET(pdz_surface_mission_slot))return true;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  return surface_registered&&Readable(session,40)&&REX_LOAD_U32(session+36)==0&&
      REX_LOAD_U8(session+25)==14;
}
void TickIntroAnimation(PPCContext&,uint8_t* base){
  const bool active=intro.active && intro.seconds>=kIntroFlyStart && Readable(intro.player,76);
  intro_animation_model=active?REX_LOAD_U32(intro.player+72):0;
  // Wait through the first half of the approach, then draw into the ready pose.
  // Keep that pose through the two-second close shot and camera handoff.
  const double elapsed=intro.seconds-kIntroFlyStart;
  intro_pose_frame.store(active?float(std::clamp(
      (elapsed-kIntroApproachSeconds*.5)/(kIntroApproachSeconds*.5),0.,1.)*174.):-1.f,
                         std::memory_order_relaxed);
  static int traced=-1;
  if(intro.active && int(intro.seconds)!=traced){
    traced=int(intro.seconds);
    Trace("Intro pose clock="+std::to_string(intro.seconds)+" model="+std::to_string(intro_animation_model));
  }
}
void SpawnSurfaceLadder(PPCContext& ctx,uint8_t* base){
  if(ladder_spawn_attempted)return;
  ladder_spawn_attempted=true;
  auto call=ctx;call.r1.u32-=2048;
  const auto name=call.r1.u32+512,out=call.r1.u32+128;
  constexpr char resource[]="aid_xml_propsetup_single_mans_ladder_ladder1059403563";
  std::memcpy(base+name,resource,sizeof(resource));REX_STORE_U32(out,0);
  call.r3.u64=name;call.r4.u64=out;sub_821ED078(call,base);
  const auto xml=REX_LOAD_U32(out);
  if(!Readable(xml,16)){Trace("Surface ladder resource unavailable");return;}
  call.r3.u64=xml;sub_826478B8(call,base);const auto source=call.r3.u32;
  const auto factory=REX_LOAD_U32(0x82D6B2B0+76);
  if(!Readable(factory,8))return;
  call.r3.u64=factory;call.r4.u64=32;sub_82513DD8(call,base);
  const auto size=call.r3.u32;
  if(size<176||size>4096||!Readable(source,size)){Trace("Surface ladder invalid setup");return;}
  const auto setup=rex::Runtime::instance()->memory()->SystemHeapAlloc(size,16);
  if(!setup)return;
  surface_prop_setups.push_back(setup); // Keep native borrowed setup alive until level teardown.
  std::memcpy(base+setup,base+source,size);
  constexpr std::array<float,6> transform{13716.667f,-233.3333f,-9815.f,0.f,180.f,0.f};
  for(unsigned i=0;i<transform.size();++i)REX_STORE_U32(setup+4+i*4,std::bit_cast<uint32_t>(transform[i]));
  // Ten native sections reach the 800-unit landing below the roof stairs.
  REX_STORE_U32(setup+144,10);
  REX_STORE_U8(setup+148,0);
  call.r3.u64=factory;call.r4.u64=32;call.r5.u64=setup;call.r6.u64=1;call.r7.u64=1;
  sub_82513D00(call,base);
  const auto actor=call.r3.u32;
  Trace("Surface ventilation ladder actor="+std::to_string(actor));
  if(Readable(actor,1304)){
    call.r3.u64=actor;sub_8258BE10(call,base);
    Trace("Surface native ladder climb height="+std::to_string(call.f1.f64));
  }
}
// Use the native petrol template for health, collision, debris and explosion audio.
void SpawnSurfaceBarrels(PPCContext& ctx,uint8_t* base){
  if(barrel_spawn_attempted)return;
  barrel_spawn_attempted=true;
  auto call=ctx;call.r1.u32-=2048;
  const auto name=call.r1.u32+512,out=call.r1.u32+128;
  auto load=[&](const char* resource){
    std::memcpy(base+name,resource,std::strlen(resource)+1);
    REX_STORE_U32(out,0);
    call.r3.u64=name;call.r4.u64=out;
    sub_821ED078(call,base);
    return REX_LOAD_U32(out);
  };
  Trace("Surface loading native barrel package");
  if(!load("aid_darkpackage_surface_barrels")){
    Trace("Surface barrel package load failed");return;
  }
  const auto xml=load("aid_xml_propsetup_single_ruin_static_explodingbarrel1");
  if(!Readable(xml,16)){Trace("Surface barrel setup load failed");return;}
  call.r3.u64=xml;sub_826478B8(call,base);
  const auto source=call.r3.u32;
  const auto factory=REX_LOAD_U32(0x82D6B2B0+76);
  if(!Readable(factory,8))return;
  call.r3.u64=factory;call.r4.u64=8;sub_82513DD8(call,base);
  const auto size=call.r3.u32;
  if(size<144||size>4096||!Readable(source,size)){
    Trace("Surface barrel setup size rejected: "+std::to_string(size));return;
  }
  constexpr std::array<std::array<float,2>,3> locations{{
    {-23432.2f,-30381.33f},{-23302.2f,-30381.33f},{-23367.2f,-30261.33f}
  }};
  for(const auto& location:locations){
    const auto setup=rex::Runtime::instance()->memory()->SystemHeapAlloc(size,16);
    if(!setup)break;
    surface_prop_setups.push_back(setup);
    std::memcpy(base+setup,base+source,size);
    const float y=pdz_surface::CameraGroundHeight(camera_ground,location[0],location[1],-10000.f);
    if(!std::isfinite(y)||y< -9000.f){Trace("Surface barrel has no supporting ground");continue;}
    REX_STORE_U32(setup+4,std::bit_cast<uint32_t>(location[0]));
    REX_STORE_U32(setup+8,std::bit_cast<uint32_t>(y));
    REX_STORE_U32(setup+12,std::bit_cast<uint32_t>(location[1]));
    for(unsigned offset=16;offset<=24;offset+=4)REX_STORE_U32(setup+offset,0);
    call.r3.u64=factory;call.r4.u64=8;call.r5.u64=setup;
    call.r6.u64=1;call.r7.u64=1;
    sub_82513D00(call,base);
    Trace("Surface native barrel actor="+std::to_string(call.r3.u32)+
      " ground="+std::to_string(y));
  }
}
void TickCabinGuards(PPCContext& ctx,uint8_t* base,uint32_t player){
  for(unsigned slot=0;slot<cabin_guards.size();++slot){
    auto& guard=cabin_guards[slot];
    if(guard.positioned||!Readable(guard.actor,13040))continue;
    const auto physics=REX_LOAD_U32(guard.actor+228);
    if(!Readable(physics,340))continue;
    auto call=ctx;call.r1.u32-=1024;
    const auto position=call.r1.u32+128;
    constexpr std::array<std::array<float,3>,7> targets{{
      {-21250.f,239.5175f,-38550.f}, {-21250.f,239.5175f,-38250.f},
      {-23400.f,148.f,-30430.f}, {-23120.f,146.f,-30350.f},
      {-13820.301868f,161.666664f,-16193.143546f}, {-14026.683957f,161.666664f,-16550.083924f}, {-14116.440902f,161.666664f,-17044.496395f}
    }};
    const auto& target=targets[slot];
    for(unsigned i=0;i<3;++i)REX_STORE_U32(position+i*4,std::bit_cast<uint32_t>(target[i]));
    call.r3.u64=guard.actor;call.r4.u64=position;
    sub_8220EC58(call,base);
    REX_STORE_U32(physics+336,std::bit_cast<uint32_t>(slot?3.14159265f:0.f));
    guard.positioned=true;
    Trace("Surface cabin guard "+std::to_string(slot+1)+" positioned after native initialization");
  }
  // The nearest-door guard owns the cabin key. Do not grant it from the
  // safe-key table: collect only at the defeated carrier's actual position.
  const auto carrier=cabin_guards[4].actor;
  if(!surface_mission.State().cabin_key && Readable(carrier,13040) &&
     REX_LOAD_U32(carrier)==0x820C5BA8 &&
     std::bit_cast<float>(REX_LOAD_U32(carrier+120))<=0.f){
    std::array<float,3> delta{};
    for(unsigned k=0;k<3;++k)delta[k]=std::bit_cast<float>(REX_LOAD_U32(player+48+k*4))-
      std::bit_cast<float>(REX_LOAD_U32(carrier+48+k*4));
    if(std::hypot(delta[0],delta[2])<135.f && std::abs(delta[1])<160.f){
      surface_mission.Apply(pdz_surface::Event::CabinKeyPickedUp);
      Trace("Surface cabin key collected from defeated nearest-door guard");
    }
  }
  if(Readable(carrier,13040)){
    const std::array<float,3> origin{-13820.301868f,161.666664f,-16193.143546f};
    for(const auto& vertex:cabin_key_vertices){
      auto* target=rex::Runtime::instance()->memory()->TranslateVirtual<uint8_t*>(vertex.address);
      for(unsigned k=0;k<3;++k){
        float v=vertex.position[k]+std::bit_cast<float>(REX_LOAD_U32(carrier+48+k*4))-origin[k];
        if(surface_mission.State().cabin_key && k==1)v=-100000.f;
        const uint32_t bits=std::bit_cast<uint32_t>(v);
        for(unsigned b=0;b<4;++b)target[k*4+b]=uint8_t(bits>>(24-b*8));
      }
    }
  }
  const auto now=std::chrono::steady_clock::now();
  const double elapsed=cabin_dialogue_last.time_since_epoch().count()?
    std::min(.1,std::chrono::duration<double>(now-cabin_dialogue_last).count()):0.;
  cabin_dialogue_last=now;
  if(cabin_dialogue_line>=2)return;
  for(unsigned slot=0;slot<2;++slot){
    const auto& guard=cabin_guards[slot];
    if(!Readable(guard.actor,13040)||REX_LOAD_U32(guard.actor)!=0x820C5BA8)return;
    // The common prop health field is initialized by the guard constructor.
    if(std::bit_cast<float>(REX_LOAD_U32(guard.actor+120))<=0.f){
      cabin_dialogue_line=2;
      return;
    }
  }
  const float x=std::bit_cast<float>(REX_LOAD_U32(player+48));
  const float z=std::bit_cast<float>(REX_LOAD_U32(player+56));
  if(!std::isfinite(x)||!std::isfinite(z)||std::hypot(x+21250.f,z+38400.f)>1400.f)return;
  cabin_dialogue_seconds+=elapsed;
  if(cabin_dialogue_seconds<(cabin_dialogue_line?8.:.5))return;
  auto call=ctx;call.r1.u32-=1024;
  const auto name=call.r1.u32+400,cue=call.r1.u32+80;
  const char* line=cabin_dialogue_line?
    "XACT_SOUNDBANK_LV04_SPEECH_CONV_GUARD1_B":
    "XACT_SOUNDBANK_LV04_SPEECH_CONV_GUARD1_A";
  std::memcpy(base+name,line,std::strlen(line)+1);
  call.r3.u64=cue;call.r4.u64=name;
  sub_821FAC08(call,base);
  const auto actor=cabin_guards[cabin_dialogue_line].actor;
  call.r3.u64=cue;call.r4.u64=actor+48;
  call.r6.u64=actor;call.r7.u64=1;call.f1.f64=1.;
  sub_821F7070(call,base);
  Trace("Surface cabin conversation line "+std::to_string(cabin_dialogue_line+1)+" requested");
  ++cabin_dialogue_line;
}
bool SurfaceUnlocked(PPCContext& ctx,uint8_t* base){
  if(REXCVAR_GET(pdz_unlock_all_missions))return true;
  if(REXCVAR_GET(pdz_surface_debug_unlock))return true;
  auto call=ctx;call.r1.u32-=512;
  const auto selected=REX_LOAD_U8(0x82D2CA58+20);
  call.r3.u64=selected==255?0:selected;
  sub_824FB718(call,base);
  const auto profile=call.r3.u32;
  if(!Readable(profile,8036+14*12))return false;
  // Native records reserve 64 missions, with four difficulty bytes for solo
  // and four for co-op. Higher-difficulty completions normally set lower bits
  // too (824FAE30); accept either mode and tolerate older non-cumulative saves.
  std::array<std::array<uint8_t,8>,14> records{};
  for(unsigned mission=0;mission<14;++mission){
    for(unsigned difficulty=0;difficulty<8;++difficulty)
      records[mission][difficulty]=REX_LOAD_U8(profile+8036+mission*12+difficulty);
  }
  return pdz_surface::CampaignComplete(records);
}
void RegisterSurface(PPCContext& ctx,uint8_t* base){
  if(!REXCVAR_GET(pdz_surface_mission_slot)||surface_registered||!pdz_input::Supported())return;
  constexpr uint32_t table=0x82D6BAC8;
  const auto count=REX_LOAD_U32(table),solo_count=REX_LOAD_U32(table+16);
  if(count!=37||solo_count!=14){Trace("Surface slot rejected unexpected mission registry");return;}
  const auto entries=REX_LOAD_U32(table+4),definitions=REX_LOAD_U32(table+8);
  const auto rules=REX_LOAD_U32(table+12),solo=REX_LOAD_U32(table+20);
  if(!Readable(entries,count*4)||!Readable(definitions,count*4)||
     !Readable(rules,count*4)||!Readable(solo,solo_count*4))return;
  const auto host_mission=uint32_t(REXCVAR_GET(pdz_surface_host_mission));
  if(host_mission>=solo_count)return;
  const auto host_index=REX_LOAD_U32(solo+host_mission*4);
  if(host_index>=count)return;
  const auto host_entry=REX_LOAD_U32(entries+host_index*4);
  const auto host_definition=REX_LOAD_U32(definitions+host_index*4);
  if(!Readable(host_entry,8)||!Readable(host_definition,264))return;
  // Independent native allocations preserve the registry's normal allocator
  // ownership. Cloned definitions borrow immutable, process-lifetime assets.
  std::array<uint32_t,6> blocks{};
  const std::array<uint32_t,6> sizes{(count+1)*4,(count+1)*4,(count+1)*4,60,8,264};
  for(unsigned i=0;i<blocks.size();++i){
    auto call=ctx;call.r1.u32-=512;sub_82395620(call,base);
    call.r4.u64=call.r3.u32;call.r3.u64=sizes[i];sub_823952C8(call,base);
    blocks[i]=call.r3.u32;
    if(!blocks[i]){
      for(const auto block:blocks)if(block){call.r3.u64=block;sub_82395428(call,base);}
      return;
    }
  }
  const std::array<uint32_t,6> sources{entries,definitions,rules,solo,host_entry,host_definition};
  const std::array<uint32_t,6> lengths{count*4,count*4,count*4,56,8,264};
  for(unsigned i=0;i<blocks.size();++i)std::memcpy(base+blocks[i],base+sources[i],lengths[i]);
  REX_STORE_U8(blocks[4]+4,14);REX_STORE_U8(blocks[4]+5,0);
  REX_STORE_U32(blocks[5]+8,kSurfaceTextBase+5);
  REX_STORE_U32(blocks[5]+12,kSurfaceTextBase+6);
  REX_STORE_U8(blocks[5]+256,14);
  REX_STORE_U32(blocks[0]+count*4,blocks[4]);
  REX_STORE_U32(blocks[1]+count*4,blocks[5]);
  REX_STORE_U32(blocks[2]+count*4,0);
  REX_STORE_U32(blocks[3]+14*4,count);
  REX_STORE_U32(table+4,blocks[0]);REX_STORE_U32(table+8,blocks[1]);
  REX_STORE_U32(table+12,blocks[2]);REX_STORE_U32(table+20,blocks[3]);
  REX_STORE_U32(table,count+1);REX_STORE_U32(table+16,15);
  surface_registered=true;
  Trace("Surface registered as native Mission 14 using host mission "+std::to_string(host_mission)+"; original 14 slots preserved");
}
void AssignNativeText(PPCContext& ctx,uint8_t* base,uint32_t target,const char* text){
  const auto length=std::strlen(text);
  auto call=ctx;call.r1.u32-=1024;
  const auto scratch=call.r1.u32+256;
  if(length>=512||!Readable(scratch,512))return;
  std::memcpy(base+scratch,text,length+1);
  call.r3.u64=target;call.r4.u64=scratch;
  sub_8261F898(call,base); // Native ASCII-to-wide string assignment/ownership.
}
void PrepareObjectivePreview(PPCContext& ctx,uint8_t* base,uint32_t level,uint32_t table_offset=996){
  if(!REXCVAR_GET(pdz_surface_objectives_test)||!Readable(level,table_offset+4))return;
  const auto objects=REX_LOAD_U32(level+table_offset);
  // The menu indexes six embedded 52-byte objective records at level+652.
  if(objects!=level+652||!Readable(objects,6*52))return;
  if(!objective_records)
    objective_records=rex::Runtime::instance()->memory()->SystemHeapAlloc(48,16);
  if(!objective_records)return;
  std::memset(base+objective_records,0,48);
  surface_mission.Reset();
  for(unsigned i=0;i<5;++i){
    const auto object=objects+i*52;
    AssignNativeText(ctx,base,object+4,pdz_surface::kObjectiveNames[i].data());
    REX_STORE_U32(object,1); // Native incomplete/active state.
    REX_STORE_U32(object+16,0); // Primary objective.
    REX_STORE_U8(object+20,1);REX_STORE_U8(object+21,0);
    REX_STORE_U32(object+24,i);
    for(unsigned j=0;j<6;++j)REX_STORE_U32(object+28+j*4,j==0?1:0);
    REX_STORE_U32(objective_records+i*8,kSurfaceTextBase+i);
  }
  REX_STORE_U32(objects+5*52,0);
  surface_objectives=objects;
  Trace("Surface native objective preview initialized (world events not connected)");
}
// A small native texture retained with the terrain buffers. Bind it only for
// this branch's colour pass. Mansion uses tf3 for diffuse and tf2 for lighting.
uint32_t SnowTexture(PPCContext& ctx,uint8_t* base,unsigned kind=0){
  // 0: albedo atlas, 1: neutral illumination, 2: flat tangent normal,
  // 3: non-emissive, non-specular material channels.
  static std::array<uint32_t,4> textures{};
  static std::array<bool,4> attempts{};
  if(kind>=textures.size())return 0;
  const bool lighting=kind!=0;
  auto& texture=textures[kind];
  auto& attempted=attempts[kind];
  if(attempted)return texture;
  attempted=true;
  // Match the loaded terrain's UV layout. v3 retains its original4x2 atlas;
  // v4 keeps all eight material indices and adds the Klobb body in tile8.
  uint32_t atlas_rows=2;
  {
    std::array<uint8_t,48> header{};
    std::ifstream input(utils::RepoRoot()/"out/surface/terrain.pdzsurf",std::ios::binary);
    input.read(reinterpret_cast<char*>(header.data()),header.size());
    if(input && !std::memcmp(header.data(),"PDZSURF1",8) &&
       header[44]==0 && header[45]==0 && header[46]==0 && header[47]==4)atlas_rows=4;
  }
  auto call=ctx;call.r1.u32-=1024;
  if(!Readable(call.r1.u32,1024)){Trace("Surface atlas failed: unreadable guest stack");return 0;}
  uint32_t size=0,allocated=0;
  for(const uint32_t candidate:{1024u,512u,128u}){
    if(lighting && candidate!=128u)continue;
    call=ctx;call.r1.u32-=1024;
    call.r3.u64=candidate;call.r4.u64=candidate;call.r5.u64=1;call.r6.u64=1;
    call.r7.u64=0;call.r8.u64=0x18280086;call.r9.u64=1;call.r10.u64=3;
    allocating_cabin_atlas=!lighting;
    sub_826DD008(call,base);
    allocating_cabin_atlas=false;
    allocated=call.r3.u32;
    Trace("Surface atlas allocation: size="+std::to_string(candidate)+
      " texture="+std::to_string(allocated));
    if(allocated){size=candidate;break;}
  }
  if(!allocated){Trace("Surface atlas failed: native texture allocation returned zero");return 0;}
  const auto locked=call.r1.u32+80;
  REX_STORE_U32(locked,0);REX_STORE_U32(locked+4,0);
  call.r3.u64=allocated;call.r4.u64=0;call.r5.u64=locked;
  call.r6.u64=0;call.r7.u64=0;sub_826DCD48(call,base);
  const auto pitch=REX_LOAD_U32(locked),data=REX_LOAD_U32(locked+4);
  Trace("Surface atlas LockRect: texture="+std::to_string(allocated)+
    " result="+std::to_string(call.r3.u32)+" pitch="+std::to_string(pitch)+
    " data="+std::to_string(data));
  if(!data||pitch<size*4||pitch>size*16||!Readable(data,pitch*size)){
    Trace("Surface atlas failed: invalid LockRect data/range");
    call.r3.u64=allocated;sub_826D2208(call,base);return 0;
  }
  // LockRect can return a physical-heap address. It must be translated like
  // the terrain vertex buffers, rather than written through base+address.
  auto* pixels=rex::Runtime::instance()->memory()->TranslateVirtual<uint8_t*>(data);
  std::array<std::vector<uint8_t>,2> cabin_pixels;
  for(unsigned i=0;i<2;++i){
    std::ifstream input(utils::RepoRoot()/(i?"mod_assets/surface/cabin-floor.rgba":"mod_assets/surface/cabin-wall.rgba"),std::ios::binary|std::ios::ate);
    if(input && input.tellg()==256*256*4){
      cabin_pixels[i].resize(256*256*4);input.seekg(0);
      input.read(reinterpret_cast<char*>(cabin_pixels[i].data()),cabin_pixels[i].size());
      if(!input)cabin_pixels[i].clear();
    }
  }
  std::vector<uint8_t> klobb_pixels;
  if(!lighting && atlas_rows==4){
    std::ifstream input(utils::RepoRoot()/"mod_assets/surface/klobb-body.rgba",std::ios::binary|std::ios::ate);
    if(input && input.tellg()==512*512*4){
      klobb_pixels.resize(512*512*4);input.seekg(0);
      input.read(reinterpret_cast<char*>(klobb_pixels.data()),klobb_pixels.size());
      if(!input)klobb_pixels.clear();
    }
    if(klobb_pixels.empty())Trace("Surface Klobb texture missing/invalid: expected 512x512 RGBA");
  }
  for(uint32_t y=0;y<size;++y)for(uint32_t x=0;x<size;++x){
    const auto noise=[](uint32_t a,uint32_t b){
      uint32_t h=a*374761393u+b*668265263u+0x9e3779b9u;
      h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&255)/255.f;
    };
    const uint32_t cell_x=size/4,cell_y=size/atlas_rows;
    const uint32_t u=(x%cell_x)*256/cell_x,v=(y%cell_y)*256/cell_y;
    const uint32_t tile=(x/cell_x)+(y/cell_y)*4;
    const float grain=noise(u,v), broad=noise(u/12,v/12);
    float r,g,b;
    if(tile==0){
      const float ridge=std::sin(float(u)*.09f+std::sin(float(v)*.045f)*2.f);
      r=210+grain*17+broad*10+ridge*7;g=r+5;b=r+10;
    }else if(tile==1){
      r=133+grain*30+broad*20;g=r+7;b=r+13;
    }else if(tile==2){
      const bool mortar=v%64<3 || (u+(v/64%2)*64)%128<3;
      r=mortar?80.f:137+grain*22+broad*12;g=r+3;b=r+6;
    }else if(tile==3){
      // Narrow upright boards, with lengthwise grain and faded blue-grey
      // weathering like the Surface timber cabins. No horizontal brick seams.
      const bool seam=u%8==0;
      const float board=noise(u/8,0),streak=noise(u,v/48);
      const float fibre=std::sin(float(u)*2.1f+std::sin(float(v)*.035f)*.7f);
      const float weather=noise(u/2,v/80);
      r=seam?34.f:82+board*35+grain*12+streak*14+fibre*9;
      g=seam?31.f:r*.86f+weather*19;
      b=seam?29.f:r*.68f+weather*38;
    }else if(tile==4){
      r=22+grain*9;g=48+grain*16+broad*7;b=35+grain*10;
    }else if(tile==5 || tile==8){
      r=67+grain*10;g=r+8;b=r+13;
    }else if(tile==6){
      r=185+grain*12;g=r+8;b=r+16;
    }else{
      r=51+grain*17;g=r*.77f;b=r*.55f;
    }
    if(lighting)r=g=b=190.f;
    if(kind==2){r=g=128.f;b=255.f;}
    if(kind==3)r=g=b=0.f;
    const int cabin_tile=tile==3?0:(tile==7?1:-1);
    if(!lighting && cabin_tile>=0 && !cabin_pixels[cabin_tile].empty()){
      const auto* texel=cabin_pixels[cabin_tile].data()+(v*256+u)*4;
      r=texel[0];g=texel[1];b=texel[2];
    }
    if(!lighting && tile==8 && !klobb_pixels.empty()){
      // A repeated border, not a clamped edge: source UVs extend slightly
      // beyond1. The compiler uses this same1/32-cell gutter at every atlas
      // resolution, including the allocation fallback sizes.
      const auto repeated=[](uint32_t coordinate,uint32_t cell){
        const float padding=float(cell)/32.f;
        const float uv=(float(coordinate)+.5f-padding)/(float(cell)-2.f*padding);
        return std::min(511u,uint32_t((uv-std::floor(uv))*512.f));
      };
      const auto sx=repeated(x%cell_x,cell_x),sy=repeated(y%cell_y,cell_y);
      const auto* texel=klobb_pixels.data()+(sy*512+sx)*4;
      r=texel[0];g=texel[1];b=texel[2];
    }
    const auto channel=[](float c){return uint32_t(std::clamp(c,0.f,255.f));};
    auto* pixel=pixels+y*pitch+x*4;
    pixel[0]=lighting?0:255;pixel[1]=uint8_t(channel(r));
    pixel[2]=uint8_t(channel(g));pixel[3]=uint8_t(channel(b));
  }
  call.r3.u64=allocated;call.r4.u64=0;sub_826DCD60(call,base);
  texture=allocated;Trace("Surface material atlas uploaded using translated LockRect memory: "+
    std::to_string(data)+" pitch "+std::to_string(pitch)+" rows "+std::to_string(atlas_rows));return texture;
}
bool Named(uint8_t* base,uint32_t node,const char* name) {
  if(!Readable(node,80))return false;
  const auto p=REX_LOAD_U32(node);
  return Readable(p,uint32_t(std::strlen(name)+1)) && !std::strcmp(reinterpret_cast<char*>(base+p),name);
}
struct Branch {uint32_t shader{},vertex{},parameters{},draw{};};
int VertexLayout(uint8_t* base,uint32_t vertex){
  if(!Readable(vertex,60))return -1;
  const auto streams=REX_LOAD_U32(vertex+48),layouts=REX_LOAD_U32(vertex+52);
  if(REX_LOAD_U32(vertex+56)!=1 || !Readable(streams,20) ||
     REX_LOAD_U32(streams)!=36 || !Readable(layouts,12))return -1;
  const auto descriptors=REX_LOAD_U32(layouts+4);
  if(!Readable(descriptors,4))return -1;
  const auto descriptor=REX_LOAD_U32(descriptors);
  if(!Readable(descriptor,68))return -1;
  const auto declaration=REX_LOAD_U32(descriptor+28);
  if(Readable(declaration,48) && REX_LOAD_U32(declaration)==0 &&
    REX_LOAD_U32(declaration+4)==0x002a23b9 &&
    REX_LOAD_U32(declaration+12)==12 && REX_LOAD_U32(declaration+16)==0x002a2187 &&
    REX_LOAD_U32(declaration+24)==16 && REX_LOAD_U32(declaration+28)==0x001a2086)return 0;
  // Mansion's opaque 36-byte stream: UV, position, tangent, bitangent,
  // normal, colour. Verify every element before selecting this upload path.
  constexpr std::array<uint32_t,18> mansion{
    0,0x002c23a5,0x00050000,8,0x002a23b9,0,
    20,0x002a2187,0x00060000,24,0x002a2187,0x00070000,
    28,0x002a2187,0x00030000,32,0x001a2086,0x000a0000};
  if(!Readable(declaration,72))return -1;
  for(unsigned i=0;i<mansion.size();++i)
    if(REX_LOAD_U32(declaration+i*4)!=mansion[i])return -1;
  return 1;
}
bool Find(uint8_t* base,uint32_t node,Branch path,Branch& result,std::unordered_set<uint32_t>& seen){
  while(node && seen.size()<12000 && seen.insert(node).second && Readable(node,80)){
    auto next=path;
    if(Named(base,node,"ndShader2"))next={node,0,0,0};
    if(Named(base,node,"ndVertexBuffer2")){
      next.vertex=VertexLayout(base,node)>=0?node:0;
    }
    if(Named(base,node,"ndShaderParam2"))next.parameters=node;
    if(Named(base,node,"ndPushBuffer") && next.shader && next.vertex && next.parameters){
      const auto materials=REX_LOAD_U32(next.shader+48);
      if(Readable(materials,12)){
        const auto table=REX_LOAD_U32(materials+4);
        if(Readable(table,4)){
          const auto material=REX_LOAD_U32(table);
          // Type 2 on the first compatible branch is alpha-tested signage.
          // Reusing it discards most snow pixels. Use a native opaque branch.
          if(Readable(material,20) && REX_LOAD_U32(material)==0){
            next.draw=node;result=next;return true;
          }
        }
      }
    }
    if(Find(base,REX_LOAD_U32(node+20),next,result,seen))return true;
    node=REX_LOAD_U32(node+24);
  }
  return false;
}
struct Payload {std::vector<uint8_t> bytes;uint32_t vertices{},triangles{},vb{},ib{},hkh{},links{};};
Payload ReadPayload(){
  Payload p;std::ifstream f(utils::RepoRoot()/"out/surface/terrain.pdzsurf",std::ios::binary|std::ios::ate);
  if(!f || f.tellg()<64 || f.tellg()>16*1024*1024)return p;
  p.bytes.resize(size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(p.bytes.data()),p.bytes.size());
  auto u=[&](size_t o){return uint32_t(p.bytes[o])<<24|uint32_t(p.bytes[o+1])<<16|uint32_t(p.bytes[o+2])<<8|p.bytes[o+3];};
  if(!f || std::memcmp(p.bytes.data(),"PDZSURF1",8) || (u(44)!=2 && u(44)!=3 && u(44)!=4))return {};
  p.vertices=u(8);p.triangles=u(12);p.vb=u(16);p.ib=u(20);p.hkh=u(24);p.links=u(28);
  if(!p.vertices || p.vertices>65535 || !p.triangles || p.triangles>100000 || p.vb!=p.vertices*36 ||
     p.ib!=p.triangles*6 || p.hkh<144 || p.links>64 ||
     64ull+p.vb+p.ib+p.hkh+p.links*4!=p.bytes.size())return {};
  for(uint32_t i=0;i<p.triangles*3;++i){
    const auto* ix=p.bytes.data()+64+p.vb+i*2;
    if((uint32_t(ix[0])<<8|ix[1])>=p.vertices)return {};
  }
  return p;
}
// Test allocations are deliberately retained until process exit: native GPU
// work and Havok bodies can outlive the level callback. Bound the test session
// to four loads rather than free memory still referenced by either subsystem.
unsigned loads=0;
uint32_t Prepare(PPCContext& ctx,uint8_t* base,uint32_t background){
  Trace("Preparing native Surface terrain");
  if(loads>=4 || !Readable(background,16))return 0;
  const uint32_t original=REX_LOAD_U32(background);
  if(!Readable(original,144))return 0;
  auto roots=REX_LOAD_U32(original+36),count=REX_LOAD_U32(original+40);
  if(count>128 || !Readable(roots,count*4))return 0;
  Branch branch;std::unordered_set<uint32_t> seen;
  for(uint32_t i=0;i<count&&!branch.draw;i++)Find(base,REX_LOAD_U32(roots+i*4),{},branch,seen);
  if(!branch.draw){Trace("No compatible native branch; original background retained");REXLOG_WARN("Surface: no compatible native terrain branch ({} nodes)",seen.size());return 0;}
  auto payload=ReadPayload();if(payload.bytes.empty()){REXLOG_WARN("Surface: missing/invalid terrain payload");return 0;}
  // Verify the expected UV, float3 position and packed tangent vertex layout.
  auto layouts=REX_LOAD_U32(branch.vertex+52);
  if(!Readable(layouts,12))return 0;
  auto descriptors=REX_LOAD_U32(layouts+4);if(!Readable(descriptors,4))return 0;
  auto descriptor=REX_LOAD_U32(descriptors);if(!Readable(descriptor,68))return 0;
  const auto vertex_layout=VertexLayout(base,branch.vertex);
  if(vertex_layout<0){
    Trace("Vertex declaration mismatch; original background retained");REXLOG_WARN("Surface: terrain vertex declaration mismatch");return 0;
  }
  auto* memory=rex::Runtime::instance()->memory();
  const uint32_t arena=memory->SystemHeapAlloc(4096+payload.hkh,64);
  const uint32_t gpu=memory->SystemHeapAlloc(payload.vb+payload.ib+256,4096,rex::memory::kSystemHeapPhysical);
  if(!arena||!gpu){if(arena)memory->SystemHeapFree(arena);if(gpu)memory->SystemHeapFree(gpu);return 0;}
  std::memset(base+arena,0,4096+payload.hkh);
  const uint32_t graph=arena+256,root=arena+512,shader=arena+768,vertex=arena+1024,
    parameters=arena+1280,draw=arena+1536,stream=arena+1792,vbh=arena+1856,
    arrays=arena+1920,ibh=arena+1984,collision=arena+4096;
  std::memcpy(base+arena,base+background,16);
  std::memcpy(base+graph,base+original,144);
  const std::array<std::pair<uint32_t,uint32_t>,4> nodes{{{shader,branch.shader},{vertex,branch.vertex},{parameters,branch.parameters},{draw,branch.draw}}};
  for(auto [to,from]:nodes){std::memcpy(base+to,base+from,128);REX_STORE_U32(to+24,0);}
  REX_STORE_U32(shader+20,vertex);REX_STORE_U32(vertex+20,parameters);
  REX_STORE_U32(parameters+20,draw);REX_STORE_U32(draw+20,0);
  REX_STORE_U32(shader+28,0);REX_STORE_U32(vertex+28,shader);
  REX_STORE_U32(parameters+28,vertex);REX_STORE_U32(draw+28,parameters);
  REX_STORE_U32(root,shader);REX_STORE_U32(graph+36,root);REX_STORE_U32(graph+40,1);
  REX_STORE_U32(arena,graph);
  // The second background graph also initializes portal bounds consumed by
  // world registration. Retain it while using a host level for bring-up.
  // Opaque PDZ stream: position 0, normal 12, colour 16, UV 20,
  // tangent 28, bitangent 32. The interchange stream has UV first.
  for(uint32_t i=0;i<payload.vertices;i++){
    const auto* source=payload.bytes.data()+64+i*36;
    // Physical 4 KB heaps have a host offset on Windows. Raw base+guest
    // addressing uploads one page before the address consumed by the GPU.
    auto* target=memory->TranslateVirtual<uint8_t*>(gpu)+i*36;
    if(vertex_layout==1){
      std::memcpy(target,source,20);
      std::memcpy(target+20,source+28,8);
      std::memcpy(target+28,source+20,4);
      std::memcpy(target+32,source+24,4);
    }else{
      std::memcpy(target,source+8,20);
      std::memcpy(target+20,source,8);
      std::memcpy(target+28,source+28,8);
    }
  }
  const uint32_t indexData=(gpu+payload.vb+127)&~127u;
  std::memcpy(memory->TranslateVirtual<uint8_t*>(indexData),payload.bytes.data()+64+payload.vb,payload.ib);
  cabin_key_vertices.clear();
  auto header_word=[&](unsigned o){const auto* b=payload.bytes.data()+o;return uint32_t(b[0])<<24|uint32_t(b[1])<<16|uint32_t(b[2])<<8|b[3];};
  klobb_pickup_vertices.clear();
  klobb_pickup_hidden=false;
  const auto klobb_range=header_word(60);
  const auto klobb_begin=klobb_range>>16,klobb_count=klobb_range&0xffffu;
  if(header_word(44)>=3 && klobb_count && uint64_t(klobb_begin)+klobb_count<=payload.triangles){
    std::unordered_set<uint32_t> seen;
    for(uint32_t n=klobb_begin*3;n<(klobb_begin+klobb_count)*3;++n){
      const auto* ix=payload.bytes.data()+64+payload.vb+n*2;
      const uint32_t id=uint32_t(ix[0])<<8|ix[1];
      if(id<payload.vertices && seen.insert(id).second)
        klobb_pickup_vertices.push_back(gpu+id*36+(vertex_layout==1?8u:0u));
    }
  }
  const auto key_begin=header_word(52),key_count=header_word(56);
  if(header_word(44)>=3 && key_count && uint64_t(key_begin)+key_count<=payload.triangles){
    std::unordered_set<uint32_t> seen;
    for(uint32_t n=key_begin*3;n<(key_begin+key_count)*3;++n){
      const auto* ix=payload.bytes.data()+64+payload.vb+n*2;
      const uint32_t id=uint32_t(ix[0])<<8|ix[1];
      if(!seen.insert(id).second)continue;
      CabinKeyVertex vertex{gpu+id*36+(vertex_layout==1?8u:0u),{}};
      for(unsigned k=0;k<3;++k){const auto* b=payload.bytes.data()+64+id*36+8+k*4;
        vertex.position[k]=std::bit_cast<float>(uint32_t(b[0])<<24|uint32_t(b[1])<<16|uint32_t(b[2])<<8|b[3]);}
      cabin_key_vertices.push_back(vertex);
    }
  }
  const auto oldStream=REX_LOAD_U32(branch.vertex+48);
  std::memcpy(base+stream,base+oldStream,20);
  REX_STORE_U32(stream+8,vbh);REX_STORE_U32(stream+12,gpu);REX_STORE_U32(stream+16,payload.vb);
  REX_STORE_U32(vertex+48,stream);
  // The host background installs a constants-only vertex callback for its
  // compiled drawing lists. Our node walker needs the complete VB binding and
  // child traversal callback; clone the descriptor to avoid changing any host
  // material, character or prop using the same global class.
  const auto vertexClass=arena+2304,vertexHandlers=arena+2400;
  const auto oldClass=REX_LOAD_U32(vertex+16);
  std::memcpy(base+vertexClass,base+oldClass,84);
  std::memcpy(base+vertexHandlers,base+REX_LOAD_U32(oldClass+80),16);
  REX_STORE_U32(vertexHandlers,0x826FE180);
  REX_STORE_U32(vertexClass+80,vertexHandlers);
  REX_STORE_U32(vertex+16,vertexClass);
  // Shader objects/declarations are already initialized by the asset loader.
  auto call=ctx;call.r1.u32-=2048;
  call.r3.u64=payload.vb;call.r4.u64=0;call.r5.u64=0;call.r6.u64=0;call.r7.u64=vbh;
  sub_82872C68(call,base);call.r3.u64=vbh;call.r4.u64=gpu;sub_82873350(call,base);
  REX_STORE_U32(draw+48,1);REX_STORE_U32(draw+52,arrays);REX_STORE_U32(arrays,ibh);
  REX_STORE_U32(draw+60,arrays+4);REX_STORE_U32(arrays+4,indexData);
  REX_STORE_U32(draw+64,arrays+8);REX_STORE_U32(arrays+8,4);
  REX_STORE_U32(draw+68,arrays+12);REX_STORE_U32(arrays+12,payload.triangles);
  const auto* key_field=payload.bytes.data()+48;
  const uint32_t key_first=uint32_t(key_field[0])<<24|uint32_t(key_field[1])<<16|uint32_t(key_field[2])<<8|key_field[3];
  if(key_first && key_first<payload.triangles && payload.triangles-key_first==10){
    safe_key_draw_count=arrays+12;safe_key_first_triangle=key_first;
  }
  REX_STORE_U8(draw+73,0);call.r3.u64=draw;sub_82702CF8(call,base);
  if(REXCVAR_GET(pdz_surface_collision_test)){
    std::memcpy(base+collision,payload.bytes.data()+64+payload.vb+payload.ib,payload.hkh);
    const size_t rel=64+payload.vb+payload.ib+payload.hkh;
    for(uint32_t i=0;i<payload.links;i++){
      auto* p=payload.bytes.data()+rel+i*4;
      const uint32_t off=uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
      if(off>payload.hkh-4 || REX_LOAD_U32(collision+off)>=payload.hkh){
        REXLOG_ERROR("Surface: rejected collision relocation");return 0;
      }
      REX_STORE_U32(collision+off,collision+REX_LOAD_U32(collision+off));
    }
    // Host mission scripts still reference its auxiliary hit/visibility table.
    // Preserve that table while replacing the Havok triangle mesh itself.
    const auto oldCollision=REX_LOAD_U32(background+8);
    if(!Readable(oldCollision,64))return 0;
    REX_STORE_U32(collision+0x5c,REX_LOAD_U32(oldCollision+60));
    if(REXCVAR_GET(pdz_surface_host_mission)==4 &&
       REXCVAR_GET(pdz_surface_snow_material_test)){
      // Mansion snow layer: part 7, triangle 4466, material 123. The live
      // surface table maps ID 24 to FSTEP_SNOW for both walking and running.
      // The overlapping upper layer uses ID 11 (concrete), not snow.
      const auto material=REX_LOAD_U32(collision+0x30);
      if(Readable(material,8)){
        REX_STORE_U32(material,0x000207ff);
        REX_STORE_U32(material+4,0x00180046);
        Trace("Surface snow material applied: surface ID 24 (FSTEP_SNOW)");
      }
    }
    REX_STORE_U32(arena+8,collision+32);
  }
  ++loads;
  camera_ground.clear();
  camera_ground.reserve(payload.triangles);
  for(uint32_t triangle=0;triangle<payload.triangles;++triangle){
    pdz_surface::CameraGroundTriangle t{};
    std::array<float,3>* points[]{&t.a,&t.b,&t.c};
    bool finite=true;
    for(unsigned vertex=0;vertex<3;++vertex)for(unsigned axis=0;axis<3;++axis){
      const auto* ix=payload.bytes.data()+64+payload.vb+(triangle*3+vertex)*2;
      const auto index=uint32_t(ix[0])<<8|ix[1];
      const auto* p=payload.bytes.data()+64+index*36+8+axis*4;
      const float value=std::bit_cast<float>(uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]);
      (*points[vertex])[axis]=value;finite=finite&&std::isfinite(value);
    }
    if(!finite)continue;
    t.min_x=std::min({t.a[0],t.b[0],t.c[0]});t.max_x=std::max({t.a[0],t.b[0],t.c[0]});
    t.min_z=std::min({t.a[2],t.b[2],t.c[2]});t.max_z=std::max({t.a[2],t.b[2],t.c[2]});
    camera_ground.push_back(t);
  }
  for(unsigned k=0;k<3;k++){
    const auto* p=payload.bytes.data()+32+k*4;
    spawn[k]=std::bit_cast<float>(uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]);
  }
  host_background=background;test_background=arena;spawned_player=0;
  Trace("Native Surface buffers ready: "+std::to_string(payload.triangles)+" triangles, background "+std::to_string(arena));
  REXLOG_INFO("Surface native terrain: {} triangles, background {:08X}, graph {:08X}, GPU {:08X}, collision {}",payload.triangles,arena,graph,gpu,REXCVAR_GET(pdz_surface_collision_test));
  return arena;
}
}
namespace pdz_surface {
bool AllocatingCabinAtlas(){return allocating_cabin_atlas;}
void RequestIntroReplay(){intro_replay_requested.store(true);}
void RequestIntroTitleSkip(){intro_title_skip_requested.store(true);}
float IntroFadeOpacity(){return intro_fade_opacity.load(std::memory_order_relaxed);}
bool EndingCameraActive(){return ending.active && !ending.dispatched;}
bool IntroCameraActive(){return intro_presentation_active.load(std::memory_order_relaxed);}
int IntroCaption(){return intro_caption.load(std::memory_order_relaxed);}
float ObjectiveNotificationSeconds(){return objective_notification_seconds.load(std::memory_order_relaxed);}
void HandleHostCutscene(PPCContext& ctx,uint8_t* base,uint32_t object){
  if(host_intro_skipped || !test_background || !SurfaceRequested(base) ||
     REXCVAR_GET(pdz_surface_host_mission)!=4 || !REXCVAR_GET(pdz_surface_intro_test) ||
     !Readable(object,16) || REX_LOAD_U32(object+12)!=3)return;
  // This is the same native operation used after the cutscene skip-button
  // query succeeds. Preserve its actor, audio and script cleanup callbacks.
  auto call=ctx;call.r1.u32-=1024;call.r3.u64=object;call.r4.u64=1;
  sub_82219660(call,base);
  host_intro_skipped=true;
  Trace("Surface inherited Mansion opening requested native skip");
}
bool Selected(uint8_t* base){
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  return surface_registered&&Readable(session,40)&&REX_LOAD_U32(session+36)==0&&
      REX_LOAD_U8(session+25)==14;
}
void Tick(PPCContext& ctx,uint8_t* base){
  pdz_weather::Tick(ctx,base);
  TickDisco(ctx,base);
  if(!test_background || !REXCVAR_GET(pdz_surface_collision_test))return;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(Readable(session,40)&&REX_LOAD_U8(session)){
    if(ending.active)ending.last=std::chrono::steady_clock::now();
    if(ending.dispatched)intro_fade_opacity=0.f;
    music.Pause();
    SetNativeMusicMuted(ctx,base,false);
    return;
  }
  const auto state=REX_LOAD_U32(0x82D6B2BC);
  if(!Readable(state,20)||REX_LOAD_U32(state+16)!=test_background)return;
  if(!ending.dispatched)music.Tick();
  SetNativeMusicMuted(ctx,base,music.stream!=nullptr);
  const auto local=REX_LOAD_U32(0x82E35468);
  if(!Readable(local,956))return;
  const auto player=REX_LOAD_U32(local+952);
  if(!Readable(player,6584))return;
  if(!intro.active && SurfaceRequested(base)){
    std::array<float,3> p{};
    for(unsigned k=0;k<3;++k)p[k]=std::bit_cast<float>(REX_LOAD_U32(player+48+k*4));
    if(!ending.active && ending.entry.Update(p)){
      ending.player=player;ending.active=true;ending.seconds=0;
      ending.last=std::chrono::steady_clock::now();
      ending.previous_lock=REX_LOAD_U8(player+0x16A3);
      REX_STORE_U8(player+0x16A3,1);
      surface_mission.Apply(Event::FellIntoVentilation);
      if(surface_objectives && Readable(surface_objectives,5*52))
        REX_STORE_U32(surface_objectives+4*52,2);
      Trace("Surface ventilation entry: ending camera started");
    }
  }
  if(ending.active){
    const auto now=std::chrono::steady_clock::now();
    ending.seconds+=std::min(.1,std::chrono::duration<double>(now-ending.last).count());
    ending.last=now;
    intro_fade_opacity=EndingFade(ending.seconds);
    if(ending.seconds>=2.5 && !ending.dispatched){
      // The retail scripts set bit 0 for extraction and bit 1 for failure.
      // The native mission update consumes these flags and shows its results.
      // Do this only after the camera has faded completely to black.
      const bool success=surface_mission.State().result==Result::Success;
      const uint8_t flags=REX_LOAD_U8(0x82E41930);
      REX_STORE_U8(0x82E41930,uint8_t((flags&~3u)|(success?1u:2u)));
      REX_STORE_U8(player+0x16A3,ending.previous_lock);
      ending.dispatched=true;
      music.Stop();
      SetNativeMusicMuted(ctx,base,false);
      if(!success){
        auto call=ctx;call.r1.u32-=1024;
        sub_82228C48(call,base); // Retail failure music transition.
      }
      Trace(success?"Surface ending dispatched native success":"Surface ending dispatched native failure: incomplete objectives");
    }
    if(ending.dispatched && ending.seconds>=2.8)intro_fade_opacity=0.f;
    return;
  }
  if(intro_replay_requested.exchange(false) && SurfaceRequested(base)){
    intro_fade_opacity=0.f;
    if(intro.active && intro.started && Readable(intro.player,0x16A4))
      REX_STORE_U8(intro.player+0x16A3,intro.previous_lock);
    intro={player,-2,std::chrono::steady_clock::now(),true,REX_LOAD_U8(player+0x16A3),false};
    intro_presentation_active=true;
    for(unsigned k=0;k<3;++k)intro.anchor[k]=std::bit_cast<float>(REX_LOAD_U32(player+48+k*4));
    const auto replay_physics=REX_LOAD_U32(player+228);
    if(Readable(replay_physics,340))intro.yaw=std::bit_cast<float>(REX_LOAD_U32(replay_physics+336));
    Trace("Surface intro manually queued from F6");
  }
  TickIntroAnimation(ctx,base);
  if(REX_LOAD_U8(player+0x16A3))return;
  SpawnSurfaceLadder(ctx,base);
  SpawnSurfaceBarrels(ctx,base);
  TickCabinGuards(ctx,base,player);
  pdz_klobb::Tick(ctx,base,player);
  if(pdz_klobb::Collected() && !klobb_pickup_hidden){
    // Only the collectible's authored vertex range is moved out of view;
    // collision never contains this small display mesh.
    const auto bits=std::bit_cast<uint32_t>(-100000.f);
    for(const auto address:klobb_pickup_vertices){
      auto* target=rex::Runtime::instance()->memory()->TranslateVirtual<uint8_t*>(address+4);
      for(unsigned b=0;b<4;++b)target[b]=uint8_t(bits>>(24-b*8));
    }
    klobb_pickup_hidden=true;
  }
  const float notification=objective_notification_seconds.load(std::memory_order_relaxed);
  if(notification>0.f){
    const float elapsed=float(ctx.f1.f64);
    if(std::isfinite(elapsed)&&elapsed>0.f&&elapsed<.5f)
      objective_notification_seconds=std::max(0.f,notification-elapsed);
  }
  if(safe_key_draw_count && surface_mission.Get(Objective::SafeKey)==Status::Incomplete){
    const float x=std::bit_cast<float>(REX_LOAD_U32(player+48));
    const float y=std::bit_cast<float>(REX_LOAD_U32(player+52));
    const float z=std::bit_cast<float>(REX_LOAD_U32(player+56));
    // Automatic proximity collection, confined to the cabin interior. The
    // table is well away from the exterior walls, preventing through-wall pickup.
    if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
       std::hypot(x+20865.f,z+38510.f)<135.f && y>200.f && y<340.f &&
       surface_mission.State().cabin_key){
      surface_mission.Apply(Event::CabinUnlocked);
      if(surface_mission.Apply(Event::SafeKeyPickedUp)){
        REX_STORE_U32(safe_key_draw_count,safe_key_first_triangle);
        objective_notification_seconds=6.f;
        // Mirror the native evidence/key pickup handler (sub_825666C8):
        // resolve STOCK_PICKUP_EVIDENCE_PROP, then play its non-positional
        // one-shot at the native gain. Use separate guest stack scratch so
        // the caller's registers and stack frame remain intact.
        auto pickup_audio=ctx;
        pickup_audio.r1.u32-=1024;
        const uint32_t cue=pickup_audio.r1.u32+80;
        pickup_audio.r3.u64=cue;
        pickup_audio.r4.u64=0x820C4A68;
        sub_821FAC08(pickup_audio,base);
        pickup_audio.r3.u64=cue;
        pickup_audio.r4.u64=0;
        pickup_audio.r6.u64=0;
        pickup_audio.r7.u64=0;
        pickup_audio.f1.f64=std::bit_cast<float>(REX_LOAD_U32(0x820E0F78));
        sub_821F7070(pickup_audio,base);
        Trace("Surface native pickup sound requested");
        Trace("Surface safe key collected from cabin table; Objective B complete");
      }
    }
  }
  if(surface_objectives && Readable(surface_objectives,6*52)){
    // DataCore's remaining script flags are not Surface completion events.
    // Keep the preview's native statuses owned by the Surface controller.
    for(unsigned i=0;i<5;++i){
      const auto status=surface_mission.Get(static_cast<Objective>(i));
      const uint32_t native=status==Status::Complete?2:status==Status::Failed?3:1;
      REX_STORE_U32(surface_objectives+i*52,native);
    }
  }
  // Defer until gameplay so constructors and level registration have finished.
  // Use the same death routine called by DataCore's fallen-spiderbot cleanup.
  auto bots=std::move(surface_spiderbots);
  surface_spiderbots.clear();
  for(const auto bot:bots){
    if(!Readable(bot,3568)||REX_LOAD_U32(bot)!=0x820CB240)continue;
    auto call=ctx;call.r1.u32-=1024;
    call.r3.u64=bot;sub_825BB150(call,base);
    if(call.r3.u32&0xff)continue;
    call=ctx;call.r1.u32-=1024;call.r3.u64=bot;call.r4.u64=0;
    sub_825BAF78(call,base);
    Trace("Surface startup spiderbot killed: "+std::to_string(bot));
  }
  if(player==spawned_player)return;
  spawn=kSurfaceStart;
  const bool cabin_preview=std::filesystem::exists(utils::RepoRoot()/"out/surface/cabin-preview.flag");
  if(cabin_preview)spawn={-13571.430664f,161.595749f,-17441.962891f};
  const auto physics=REX_LOAD_U32(player+228);
  if(!Readable(physics,4)||!std::isfinite(spawn[0])||!std::isfinite(spawn[1])||!std::isfinite(spawn[2]))return;
  auto call=ctx;call.r1.u32-=512;
  if(!Readable(call.r1.u32,512))return;
  const auto position=call.r1.u32+128;
  for(unsigned k=0;k<3;k++)REX_STORE_U32(position+k*4,std::bit_cast<uint32_t>(spawn[k]));
  call.r3.u64=player;call.r4.u64=position;
  sub_8220EC58(call,base); // Native character-position helper used by scripted transforms.
  if(Readable(physics,340))REX_STORE_U32(physics+336,std::bit_cast<uint32_t>(kSurfaceStartYaw));
  REX_STORE_U32(player+0x1674,std::bit_cast<uint32_t>(kSurfaceStartPitch));
  spawned_player=player;
  if(REXCVAR_GET(pdz_surface_intro_test) && !cabin_preview){
    intro={player,0,std::chrono::steady_clock::now(),true,REX_LOAD_U8(player+0x16A3),false};
    intro_presentation_active=true;
    intro.anchor=spawn;
    Trace("Surface opening queued immediately at mission spawn");
  }
  Trace("Surface test spawn applied to local character "+std::to_string(player)+
    " at "+std::to_string(spawn[0])+","+std::to_string(spawn[1])+","+std::to_string(spawn[2]));
}
}

namespace {
void TracePlayerRelocation(PPCContext& ctx,uint8_t* base,const char* setter){
  if(!test_background||!REXCVAR_GET(pdz_surface_test)||
     !REXCVAR_GET(pdz_surface_collision_test))return;
  const auto local=REX_LOAD_U32(0x82E35468);
  if(!Readable(local,956)||REX_LOAD_U32(local+952)!=ctx.r3.u32||
     !Readable(ctx.r3.u32,60)||!Readable(ctx.r4.u32,12))return;
  std::array<float,3> before{},after{};
  float distance2=0;
  for(unsigned k=0;k<3;k++){
    before[k]=std::bit_cast<float>(REX_LOAD_U32(ctx.r3.u32+48+k*4));
    after[k]=std::bit_cast<float>(REX_LOAD_U32(ctx.r4.u32+k*4));
    if(!std::isfinite(before[k])||!std::isfinite(after[k]))return;
    distance2+=(after[k]-before[k])*(after[k]-before[k]);
  }
  if(distance2<1000.f*1000.f)return;
  static unsigned reports=0;if(reports++>=32)return;
  Trace(std::string("Surface large relocation ")+setter+" caller "+std::to_string(uint32_t(ctx.lr))+
    " from "+std::to_string(before[0])+","+std::to_string(before[1])+","+std::to_string(before[2])+
    " to "+std::to_string(after[0])+","+std::to_string(after[1])+","+std::to_string(after[2]));
}
}
// Only the per-player HUD visibility call is suppressed. Menu visibility
// messages use this same native helper and must still work while paused.
REX_EXTERN(__imp__sub_8239DC80);
// Limit the retarget to this player's body evaluation. The head and attached
// prop then inherit the resulting native joint matrices normally.
REX_EXTERN(__imp__sub_821E4900);
REX_HOOK_RAW(sub_821E4900){
  const bool previous=evaluating_intro_body;
  evaluating_intro_body=pdz_surface::IntroCameraActive() &&
      intro_pose_frame.load(std::memory_order_relaxed)>=0 &&
      Readable(ctx.r3.u32,76) && REX_LOAD_U32(ctx.r3.u32+72)==intro_animation_model;
  static bool traced=false;
  if(evaluating_intro_body && !traced){Trace("Intro native body evaluation reached");traced=true;}
  __imp__sub_821E4900(ctx,base);
  if(evaluating_intro_body)pdz_surface::RefreshIntroProp(ctx,base);
  evaluating_intro_body=previous;
}
REX_EXTERN(__imp__sub_827378B0);
REX_HOOK_RAW(sub_827378B0){
  const auto state=ctx.r3.u32;
  const float frame=intro_pose_frame.load(std::memory_order_relaxed);
  if(!evaluating_intro_body || frame<0 || !Readable(state,140)){
    __imp__sub_827378B0(ctx,base);return;
  }
  const auto pose=REX_LOAD_U32(state+112),mask=REX_LOAD_U32(state+124);
  const auto count=REX_LOAD_U16(state+42);
  static unsigned diagnostics=0;
  if(diagnostics++<3)Trace("Intro pose evaluator state="+std::to_string(state)+" pose="+std::to_string(pose)+" mask="+std::to_string(mask)+" count="+std::to_string(count)+" animated="+std::to_string(REX_LOAD_U16(state+32))+" mode="+std::to_string(REX_LOAD_U16(state+44)));
  if(count<59 || count>256 || !Readable(pose,count*48) ||
     (mask && !Readable(mask,count)) || (!mask && !REX_LOAD_U32(state+128))){
    __imp__sub_827378B0(ctx,base);return;
  }
  if(!intro_animation_attempted){
    Trace("Surface GoldenEye intro pose active; native joint count="+std::to_string(count));
    intro_animation_attempted=true;
  }
  // GoldenEye anatomical tracks -> Joanna's bind skeleton. Keep the native
  // lower body and fingers; preserve her proportions and attachment hierarchy.
  constexpr std::array<std::array<unsigned,2>,8> joints{{
    {9,1},{13,2},{18,3},{21,4},{34,5},{39,6},{53,7},{58,8}
  }};
  std::array<std::array<uint8_t,48>,joints.size()> saved{};
  std::array<uint8_t,joints.size()> masks{};
  const unsigned first=std::min(174u,unsigned(frame)),next=std::min(174u,first+1);
  const float t=frame-float(first);
  for(size_t i=0;i<joints.size();++i){
    const auto joint=joints[i][0],track=joints[i][1],at=pose+joint*48;
    std::memcpy(saved[i].data(),base+at,48);
    if(mask)masks[i]=REX_LOAD_U8(mask+joint);
    const auto& a=pdz_surface::kIntroJointTracks[first][track];
    const auto& b=pdz_surface::kIntroJointTracks[next][track];
    float dot=0;for(unsigned k=0;k<4;++k)dot+=a[k]*b[k];
    std::array<float,4> q{};float length=0;
    for(unsigned k=0;k<4;++k){q[k]=a[k]*(1-t)+b[k]*t*(dot<0?-1.f:1.f);length+=q[k]*q[k];}
    length=std::sqrt(length);
    for(unsigned k=0;k<4;++k)REX_STORE_U32(at+k*4,std::bit_cast<uint32_t>(q[k]/length));
    // Animation translations are deltas from each PDZ bind joint.
    for(unsigned k=0;k<3;++k){
      REX_STORE_U32(at+16+k*4,0);
      REX_STORE_U32(at+32+k*4,std::bit_cast<uint32_t>(1.f));
    }
    if(mask)REX_STORE_U8(mask+joint,1);
  }
  __imp__sub_827378B0(ctx,base);
  // Pose storage belongs to the animation mixer; never leave our override in
  // shared scratch memory after the native world matrices have been built.
  for(size_t i=0;i<joints.size();++i){
    std::memcpy(base+pose+joints[i][0]*48,saved[i].data(),48);
    if(mask)REX_STORE_U8(mask+joints[i][0],masks[i]);
  }
}
REX_HOOK_RAW(sub_8239DC80){
  const auto caller=uint32_t(ctx.lr);
  if(pdz_surface::IntroCameraActive() && caller>=0x823A03E8 && caller<0x823A0760)
    ctx.r4.u64=0;
  __imp__sub_8239DC80(ctx,base);
}
REX_EXTERN(__imp__sub_8220EC58);
REX_HOOK_RAW(sub_8220EC58){
  TracePlayerRelocation(ctx,base,"physics");
  __imp__sub_8220EC58(ctx,base);
}
REX_EXTERN(__imp__sub_8250A0C0);
REX_HOOK_RAW(sub_8250A0C0){
  TracePlayerRelocation(ctx,base,"object");
  __imp__sub_8250A0C0(ctx,base);
}
REX_EXTERN(__imp__sub_8260B9B0);
REX_HOOK_RAW(sub_8260B9B0){
  if(pdz_input::Supported()&&SurfaceRequested(base)){
    if(auto replacement=Prepare(ctx,base,ctx.r3.u32))ctx.r3.u64=replacement;
  }
  __imp__sub_8260B9B0(ctx,base);
  if(REXCVAR_GET(pdz_surface_test))Trace("Native background initialization returned");
}

// The normal background renderer executes the precompiled command list at
// graph+132. A cloned graph retains the host list, so send only our test graph
// through the native node walker instead. All other assets retain their path.
REX_EXTERN(__imp__sub_827017F8);
REX_HOOK_RAW(sub_827017F8){
  if(test_background && REXCVAR_GET(pdz_surface_test) &&
     ctx.r3.u32==test_background+256){
    static unsigned traced=0;
    if(traced++<4)Trace("Surface node render pass "+std::to_string(ctx.r5.u32));
    ctx.r4.u64=0;
    sub_826FF850(ctx,base);
    return;
  }
  __imp__sub_827017F8(ctx,base);
}

REX_EXTERN(__imp__sub_824DB530);
REX_HOOK_RAW(sub_824DB530){
  if(test_background && ctx.r3.u32==host_background &&
     REXCVAR_GET(pdz_surface_test) && REXCVAR_GET(pdz_surface_collision_test)){
    ctx.r3.u64=test_background;
    Trace("Registering native Surface collision world");
  }
  __imp__sub_824DB530(ctx,base);
}

// The clone is not registered in the game's asset manager. Its backing host
// asset must be released through its original identity when leaving the level.
REX_EXTERN(__imp__sub_821EDB38);
REX_HOOK_RAW(sub_821EDB38){
  if(test_background && ctx.r3.u32==test_background)ctx.r3.u64=host_background;
  __imp__sub_821EDB38(ctx,base);
}
#include "coop_ally.h"
REX_EXTERN(__imp__sub_8260BD40);
REX_HOOK_RAW(sub_8260BD40){
  if(intro.active && intro.started && Readable(intro.player,0x16A4))
    REX_STORE_U8(intro.player+0x16A3,intro.previous_lock);
  intro={};
  intro_animation_model=0;intro_animation_attempted=false;intro_pose_frame.store(-1.f);
  intro_presentation_active=false;
  intro_caption=0;
  if(ending.active && Readable(ending.player,0x16A4))
    REX_STORE_U8(ending.player+0x16A3,ending.previous_lock);
  ending={};
  camera_ground.clear();
  intro_fade_opacity=0.f;
  intro_replay_requested=false;
  intro_title_skip_requested=false;
  host_intro_skipped=false;
  surface_objectives=0;
  safe_key_draw_count=safe_key_first_triangle=0;
  objective_notification_seconds=0.f;
  surface_observed_entities.clear();
  cabin_key_vertices.clear();
  klobb_pickup_vertices.clear();
  klobb_pickup_hidden=false;
  pdz_klobb::ResetPickup();
  cabin_dialogue_line=0;
  cabin_dialogue_seconds=0;
  cabin_dialogue_last={};
  surface_mission.Reset();
  surface_spiderbots.clear();
  music.Stop();
  ResetDisco();
  surface_snow_emitters.clear();
  SetNativeMusicMuted(ctx,base,false);
  __imp__sub_8260BD40(ctx,base);
  
  pdz_coop::LevelEnded();
  pdz_population::LevelEnded();
  for(auto setup:surface_prop_setups)rex::Runtime::instance()->memory()->SystemHeapFree(setup);
  surface_prop_setups.clear();barrel_spawn_attempted=false;ladder_spawn_attempted=false;
  test_background=host_background=spawned_player=0;
}

// Apply the cinematic after PDZ has resolved the ordinary camera. Reuse its
// look-at and view setters so culling, projection and rendering agree.
namespace {
void ApplySurfaceEnding(PPCContext& ctx,uint8_t* base,uint32_t camera){
  if(!pdz_surface::EndingCameraActive() || !Readable(camera,1092) ||
     REX_LOAD_U32(camera+16)!=ending.player)return;
  const auto view=REX_LOAD_U32(camera+60);
  if(!Readable(view,496))return;
  // Elevated, fixed three-quarter shot: keep the rim and Joanna's descent
  // in frame rather than following the first-person camera into the shaft.
  const std::array<float,3> position{14240.f,1450.f,-10260.f};
  const std::array<float,3> target{13716.67f,700.f,-10939.f};
  auto call=ctx;call.r1.u32-=1024;
  if(!Readable(call.r1.u32,1024))return;
  const auto pos=call.r1.u32+256,at=pos+16,up=at+16;
  for(unsigned k=0;k<4;++k){
    REX_STORE_U32(pos+k*4,std::bit_cast<uint32_t>(k<3?position[k]:0.f));
    REX_STORE_U32(at+k*4,std::bit_cast<uint32_t>(k<3?target[k]:0.f));
    REX_STORE_U32(up+k*4,std::bit_cast<uint32_t>(k==1?1.f:0.f));
  }
  call.r3.u64=camera+1024;call.r4.u64=pos;call.r5.u64=at;call.r6.u64=up;
  sub_827C6710(call,base);
  call=ctx;call.r1.u32-=1024;call.r3.u64=view;call.r4.u64=camera+1024;
  sub_82643740(call,base);
}
void ApplySurfaceIntro(PPCContext& ctx,uint8_t* base,uint32_t camera){
  if(!intro.active || !test_background || !Readable(camera,1092) ||
     REX_LOAD_U32(camera+16)!=intro.player)return;
  const auto now=std::chrono::steady_clock::now();
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  const bool paused=Readable(session,1)&&REX_LOAD_U8(session);
  if(!paused)intro.seconds+=std::min(.1,std::chrono::duration<double>(now-intro.last).count())*
      (intro.seconds>=8.15?std::clamp(REXCVAR_GET(pdz_surface_intro_preview_speed),.05,1.):1.);
  intro.last=now;
  if(REXCVAR_GET(pdz_surface_intro_pose_inspection) &&
     intro.seconds>kIntroFlyStart+kIntroApproachSeconds+kIntroAimHoldSeconds)
    intro.seconds=kIntroFlyStart+kIntroApproachSeconds*.5;
  if(intro.seconds<0 || paused)return;
  if(intro_title_skip_requested.exchange(false) && intro.seconds<7.35)
    intro.seconds=7.35; // Keep the existing fade and fly-in handoff intact.
  intro_caption=intro.seconds>=.6 && intro.seconds<4.2?1:
      intro.seconds>=4.5 && intro.seconds<8?2:0;
  if(!intro.started){
    if(!Readable(intro.player,0x16A4))return;
    intro.previous_lock=REX_LOAD_U8(intro.player+0x16A3);
    REX_STORE_U8(intro.player+0x16A3,1);
    intro.started=true;
    Trace("Surface opening preview first visible-render frame");
  }
  // The host mission can clear this during its own introduction cleanup.
  if(Readable(intro.player,0x16A4))REX_STORE_U8(intro.player+0x16A3,1);
  constexpr double establishing_seconds=8;
  // Hold the incoming camera at its first key until the black midpoint has
  // passed. The same cinematic clock drives both cameras and the fade, so
  // pausing and different frame rates cannot desynchronize the transition.
  constexpr double fade_seconds=.65, black_hold=.15;
  const double fade_out_start=establishing_seconds-fade_seconds;
  const double fly_in_start=establishing_seconds+black_hold;
  float opacity=0.f;
  if(intro.seconds>=fade_out_start && intro.seconds<establishing_seconds)
    opacity=float((intro.seconds-fade_out_start)/fade_seconds);
  else if(intro.seconds>=establishing_seconds && intro.seconds<fly_in_start)
    opacity=1.f;
  else if(intro.seconds>=fly_in_start && intro.seconds<fly_in_start+fade_seconds)
    opacity=float(1-(intro.seconds-fly_in_start)/fade_seconds);
  opacity=std::clamp(opacity,0.f,1.f);
  intro_fade_opacity.store(opacity*opacity*(3-2*opacity),std::memory_order_relaxed);
  const bool establishing=intro.seconds<establishing_seconds;
  const double fly_elapsed=std::max(0.0,intro.seconds-fly_in_start);
  double tick=std::min(fly_elapsed/kIntroApproachSeconds,1.0)*kIntroApproachTicks;
  if(fly_elapsed>kIntroApproachSeconds+kIntroAimHoldSeconds)
    tick+=(fly_elapsed-kIntroApproachSeconds-kIntroAimHoldSeconds)*
        26.0/kIntroCameraMergeSeconds;
  // Hand off at the first eye-position key, before the spline swings through Joanna.
  const bool reached_handoff=tick>=252.0;
  size_t segment=0;
  while(segment+1<pdz_surface::kIntroKeys.size() && tick>=pdz_surface::kIntroKeys[segment].ticks){
    tick-=pdz_surface::kIntroKeys[segment].ticks;++segment;
  }
  if(reached_handoff || segment+1>=pdz_surface::kIntroKeys.size()){
    if(Readable(intro.player,0x16A4))REX_STORE_U8(intro.player+0x16A3,intro.previous_lock);
    intro_fade_opacity=0.f;
    intro.active=false;intro_presentation_active=false;intro_caption=0;
    Trace("Surface opening preview returned camera control");return;
  }
  const auto& key=pdz_surface::kIntroKeys[segment];
  const float t=float(tick/key.ticks),t2=t*t,t3=t2*t,s=key.tension;
  const std::array<float,4> weights{(-t+2*t2-t3)*s,
    1+(s-3)*t2+(2-s)*t3,s*t+(3-2*s)*t2+(s-2)*t3,(t3-t2)*s};
  std::array<float,3> offset{};
  for(int k=0;k<4;++k){
    const auto index=std::clamp(int(segment)+k-1,0,int(pdz_surface::kIntroKeys.size())-1);
    for(unsigned axis=0;axis<3;++axis)offset[axis]+=weights[k]*pdz_surface::kIntroKeys[index].offset[axis];
  }
  // Original insertion pad faces -X. Rotate the character-relative spline
  // into the calibrated Surface coordinate frame; target Joanna's eye line.
  std::array<float,3> target{intro.anchor[0],intro.anchor[1]+65,intro.anchor[2]};
  const float forward_x=std::sin(intro.yaw),forward_z=std::cos(intro.yaw);
  std::array<float,3> position{
    target[0]+(offset[0]*forward_z+offset[2]*forward_x)*4.424f,
    target[1]+offset[1]*4.424f,
    target[2]+(-offset[0]*forward_x+offset[2]*forward_z)*4.424f};
  if(establishing){
    // Dish pad 10050 in the calibrated Surface setup. Frame the structure
    // from an elevated oblique angle before cutting to the insertion swirl.
    target={-5398.f*4.424f,64.f*4.424f+800.f,-6708.f*4.424f};
    position={target[0]+2400.f,target[1]+1200.f,target[2]+3200.f};
  }
  if(!establishing && REXCVAR_GET(pdz_surface_intro_pose_inspection)){
    target[1]=intro.anchor[1]+95.f;
    position={target[0]+forward_x*320.f+forward_z*100.f,target[1]+25.f,
              target[2]+forward_z*320.f-forward_x*100.f};
  }
  if(!establishing){
    const float minimum=pdz_surface::CameraMinimumHeight(camera_ground,
        position[0],position[2],intro.anchor[1]);
    position[1]=std::max(position[1],minimum);
  }
  // The source's final points coincide with the player: keep a finite view
  // direction until the ordinary first-person camera resumes.
  float distance=0;for(unsigned k=0;k<3;++k)distance+=std::pow(position[k]-target[k],2);
  if(distance<1)position[0]=target[0]+1;
  const auto view=REX_LOAD_U32(camera+60);
  if(!Readable(view,496))return;
  auto call=ctx;call.r1.u32-=1024;
  if(!Readable(call.r1.u32,1024))return;
  const auto pos=call.r1.u32+256,at=pos+16,up=at+16;
  for(unsigned k=0;k<4;++k){
    REX_STORE_U32(pos+k*4,std::bit_cast<uint32_t>(k<3?position[k]:0.f));
    REX_STORE_U32(at+k*4,std::bit_cast<uint32_t>(k<3?target[k]:0.f));
    REX_STORE_U32(up+k*4,std::bit_cast<uint32_t>(k==1?1.f:0.f));
  }
  call.r3.u64=camera+1024;call.r4.u64=pos;call.r5.u64=at;call.r6.u64=up;
  sub_827C6710(call,base);
  call=ctx;call.r1.u32-=1024;call.r3.u64=view;call.r4.u64=camera+1024;
  sub_82643740(call,base);
}
}
REX_EXTERN(__imp__sub_82643740);
REX_HOOK_RAW(sub_82643740){
  const auto destination=ctx.r3.u32;
  __imp__sub_82643740(ctx,base);
  // Enforce the cinematic at the view assignment itself. Camera update and
  // render preparation may both assign this view within a single frame.
  static thread_local bool applying=false;
  const auto cinematic_player=pdz_surface::EndingCameraActive()?ending.player:intro.player;
  if(!applying && (intro.active || pdz_surface::EndingCameraActive()) &&
     test_background && Readable(cinematic_player,6564)){
    const auto owner=REX_LOAD_U32(cinematic_player+6560);
    if(Readable(owner,1152)){
      const auto view=REX_LOAD_U32(owner+1148);
      if(Readable(view,24)){
        const auto camera=REX_LOAD_U32(view+20);
        if(Readable(camera,1092) && REX_LOAD_U32(camera+60)==destination){
          applying=true;
          if(pdz_surface::EndingCameraActive())ApplySurfaceEnding(ctx,base,camera);
          else ApplySurfaceIntro(ctx,base,camera);
          applying=false;
          static double next_report=0;
          if(intro.started && intro.seconds>=next_report){
            next_report=intro.seconds+1;
            Trace("Surface final view assignment: view "+std::to_string(destination)+
              " time "+std::to_string(intro.seconds)+" matrix translation "+
              std::to_string(std::bit_cast<float>(REX_LOAD_U32(destination+160)))+","+
              std::to_string(std::bit_cast<float>(REX_LOAD_U32(destination+164)))+","+
              std::to_string(std::bit_cast<float>(REX_LOAD_U32(destination+168))));
          }
        }
      }
    }
  }
}

REX_EXTERN(__imp__sub_825B6D80);
REX_HOOK_RAW(sub_825B6D80){
  const auto bot=ctx.r3.u32;
  __imp__sub_825B6D80(ctx,base);
  if(pdz_input::Supported() && SurfaceRequested(base) &&
     REXCVAR_GET(pdz_surface_collision_test) && surface_spiderbots.size()<512)
    surface_spiderbots.push_back(bot);
}

// DataCore initialization normally tests profile flag 4 and, for a new
// profile, installs three look-training input restrictions. Mirror its
// completed-training branch for this host test without modifying the profile.
REX_EXTERN(__imp__sub_822F1D60);
REX_HOOK_RAW(sub_822F1D60){
  const auto level=ctx.r3.u32;
  const bool bypass=test_background && REXCVAR_GET(pdz_surface_test) &&
      REXCVAR_GET(pdz_surface_collision_test);
  surface_datacore_initializing=bypass;
  __imp__sub_822F1D60(ctx,base);
  surface_datacore_initializing=false;
  if(bypass && Readable(level,996)){
    const auto flags=REX_LOAD_U32(level+992);
    if(Readable(flags,28)){
      REX_STORE_U32(flags+16,REX_LOAD_U32(flags+16)|0x30);
      REX_STORE_U32(flags+24,REX_LOAD_U32(flags+24)|0xC00);
      Trace("Surface host look tutorial bypassed");
    }
    PrepareObjectivePreview(ctx,base,level);
  }
}
// Mansion Infiltration stores its objective table after an embedded script
// callback, at +1008. +996 is executable code in this controller, not a table.
REX_EXTERN(__imp__sub_823244A8);
REX_HOOK_RAW(sub_823244A8){
  const auto level=ctx.r3.u32;
  __imp__sub_823244A8(ctx,base);
  if(test_background && SurfaceRequested(base) &&
     REXCVAR_GET(pdz_surface_host_mission)==4 && Readable(level,1012) &&
     REX_LOAD_U32(level)==0x820A5998){
    PrepareObjectivePreview(ctx,base,level,1008);
    Trace("Surface Mansion host initialized; native snow environment retained");
  }
}
// Route the existing pause menu's detail lookup to our records. Their strings
// are assigned by the native string class, never by replacing XUI with a HUD.
REX_EXTERN(__imp__sub_8214CD40);
REX_HOOK_RAW(sub_8214CD40){
  if(SurfaceObjective(ctx.r3.u32)&&ctx.r4.u32<8){
    ctx.r3.u64=objective_records+(ctx.r3.u32-surface_objectives)/52*8;
    return;
  }
  __imp__sub_8214CD40(ctx,base);
}
REX_EXTERN(__imp__sub_826400B8);
REX_HOOK_RAW(sub_826400B8){
  const auto index=ctx.r3.u32-kSurfaceTextBase;
  if(surface_registered&&(index==5||index==6)){
    const auto target=ctx.r4.u32;
    AssignNativeText(ctx,base,target,index==5?"SURFACE":"COLD STORAGE");
    ctx.r3.u64=target;
    return;
  }
  if(surface_objectives&&index<objective_instructions.size()){
    const auto target=ctx.r4.u32;
    AssignNativeText(ctx,base,target,objective_instructions[index]);
    ctx.r3.u64=target;
    return;
  }
  __imp__sub_826400B8(ctx,base);
}
REX_EXTERN(__imp__sub_82272A28);
REX_HOOK_RAW(sub_82272A28){
  __imp__sub_82272A28(ctx,base);
  RegisterSurface(ctx,base);
}
REX_EXTERN(__imp__sub_823F7C18);
REX_HOOK_RAW(sub_823F7C18){
  __imp__sub_823F7C18(ctx,base);
  if(pdz_input::Supported()&&REXCVAR_GET(pdz_unlock_all_missions)){
    const auto session=REX_LOAD_U32(0x82D6B2B0);
    if(Readable(session,40)&&REX_LOAD_U32(session+36)<=1){
      // Use the native registered mission count, not invented save progress.
      ctx.r3.u64=std::min(REX_LOAD_U32(0x82D6BAD8),surface_registered?15u:14u);
      return;
    }
  }
  if(!surface_registered)return;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(Readable(session,40)&&REX_LOAD_U32(session+36)==0&&SurfaceUnlocked(ctx,base))
    ctx.r3.u64=15;
  else ctx.r3.u64=std::min(ctx.r3.u32,14u);
}
REX_EXTERN(__imp__sub_82229160);
REX_HOOK_RAW(sub_82229160){
  if(surface_registered&&ctx.r4.u32==14&&
     (!Readable(ctx.r3.u32,40)||REX_LOAD_U32(ctx.r3.u32+36)!=0||!SurfaceUnlocked(ctx,base)))return;
  __imp__sub_82229160(ctx,base);
}
REX_EXTERN(__imp__sub_8214CAF8);
REX_HOOK_RAW(sub_8214CAF8){
  if(SurfaceObjective(ctx.r3.u32))return;
  __imp__sub_8214CAF8(ctx,base);
}
REX_EXTERN(__imp__sub_8214CC08);
REX_HOOK_RAW(sub_8214CC08){
  if(SurfaceObjective(ctx.r3.u32))return;
  __imp__sub_8214CC08(ctx,base);
}
REX_EXTERN(__imp__sub_8214CE88);
REX_HOOK_RAW(sub_8214CE88){
  if(SurfaceObjective(ctx.r3.u32))return;
  __imp__sub_8214CE88(ctx,base);
}
REX_EXTERN(__imp__sub_8253E1B8);
REX_HOOK_RAW(sub_8253E1B8){
  if(surface_datacore_initializing && Readable(ctx.r3.u32,4) &&
     REX_LOAD_U32(ctx.r3.u32)==0x820C93A0){
    ctx.r4.u64&=~(uint64_t(2)|(uint64_t(1)<<52)|(uint64_t(1)<<53));
    if(!ctx.r4.u64)return;
  }
  __imp__sub_8253E1B8(ctx,base);
}

// Apply the replacement at the final category gain setter, without pausing
// cues or changing their timing/state (cutscenes may wait for cue completion).
REX_EXTERN(__imp__sub_8274E018);
REX_HOOK_RAW(sub_8274E018){
  if(native_music_muted && MusicCategory(ctx.r3.u32 & 0xffff))ctx.r4.u64=0;
  __imp__sub_8274E018(ctx,base);
}

// Both native solo pause entry paths select MUSIC_PAUSE_PAUSE_MAIN and fade
// category 15 themselves. Restore their mixer before they select/fade cues;
// leaving our gameplay category override active also silences pause music.
REX_EXTERN(__imp__sub_821F2420);
REX_HOOK_RAW(sub_821F2420){
  if(test_background){music.Pause();SetNativeMusicMuted(ctx,base,false);}
  __imp__sub_821F2420(ctx,base);
}
REX_EXTERN(__imp__sub_82228180);
REX_HOOK_RAW(sub_82228180){
  if(test_background){music.Pause();SetNativeMusicMuted(ctx,base,false);}
  __imp__sub_82228180(ctx,base);
}

// Reuse a shipped outdoor daytime dome via the native asset loader. Copy the
// setup record onto the guest stack: never modify Mansion's shared asset data.
REX_EXTERN(__imp__sub_825B0D08);
REX_HOOK_RAW(sub_825B0D08){
  if(REXCVAR_GET(pdz_surface_test) && REXCVAR_GET(pdz_surface_daytime) &&
     SurfaceRequested(base) && Readable(ctx.r4.u32,64) &&
     Readable(ctx.r1.u32-2048,2048)){
    const auto scratch=ctx.r1.u32-512;
    constexpr char sky[]="aid_darkcharacter_testmodels_skydomedesert";
    std::memcpy(base+scratch+128,sky,sizeof(sky));
    auto lookup=ctx;lookup.r1.u32-=2048;
    lookup.r3.u64=scratch+128;lookup.r4.u64=22;lookup.r5.u64=0;
    sub_824080A8(lookup,base); // Optional lookup: no missing-asset fallback.
    if(lookup.r3.u32){
      std::memcpy(base+scratch,base+ctx.r4.u32,64);
      REX_STORE_U32(scratch+52,scratch+128);
      // Do not apply Mansion's optional sky animation to another dome.
      REX_STORE_U32(scratch+56,scratch+192);
      constexpr char none[]="_noname_";
      std::memcpy(base+scratch+192,none,sizeof(none));
      auto call=ctx;call.r1.u32-=2048;call.r4.u64=scratch;
      __imp__sub_825B0D08(call,base);ctx.r3=call.r3;
      Trace("Surface daytime dome loaded through native sky constructor");
      return;
    }
    Trace("Surface daytime dome unavailable; retained host dome");
  }
  __imp__sub_825B0D08(ctx,base);
}

// Override the resolved per-view fog, after native volume blending. Other
// missions keep their original fog records and camera settings.
REX_EXTERN(__imp__sub_82207778);
REX_HOOK_RAW(sub_82207778){
  const auto render_context=ctx.r4.u32;
  __imp__sub_82207778(ctx,base);
  // Surface's imported geometry lies outside Mansion's authored lighting
  // volumes. Supply stable, slightly brighter cool fill to the resolved view.
  // Absolute values avoid accumulating brightness through native fallback.
  if(test_background && SurfaceRequested(base) && Readable(render_context,720)){
    constexpr std::array<float,3> light{1.10f,1.14f,1.20f};
    constexpr std::array<float,3> fill{.60f,.64f,.70f};
    for(unsigned i=0;i<3;++i){
      REX_STORE_U32(render_context+692+i*4,std::bit_cast<uint32_t>(light[i]));
      REX_STORE_U32(render_context+704+i*4,std::bit_cast<uint32_t>(fill[i]));
    }
  }
  if(test_background && REXCVAR_GET(pdz_surface_daytime) &&
     Readable(render_context,684)){
    REX_STORE_U8(render_context+660,1);
    REX_STORE_U32(render_context+664,0xBBCDDDFF); // Cool daylight haze, RGBA.
    REX_STORE_U32(render_context+668,std::bit_cast<uint32_t>(8000.f));
    REX_STORE_U32(render_context+672,std::bit_cast<uint32_t>(65000.f));
    REX_STORE_U32(render_context+676,std::bit_cast<uint32_t>(100000.f));
    REX_STORE_U32(render_context+680,std::bit_cast<uint32_t>(-100000.f));
  }
}

REX_EXTERN(__imp__sub_82702E08);
// Reuse Mansion's authored world-space snow, keeping a bounded group near
// the local player. Native updates retain pause, lifetime and particle budget.
REX_EXTERN(__imp__sub_82593AE8);
REX_HOOK_RAW(sub_82593AE8){
  const auto object=ctx.r3.u32,setup=ctx.r4.u32;
  bool snow=false;
  if(SurfaceRequested(base) && REXCVAR_GET(pdz_surface_host_mission)==4 &&
     surface_snow_emitters.size()<4 && Readable(setup,120)){
    const auto name=REX_LOAD_U32(setup+60);
    constexpr char prefix[]="aid_xml_emitter_level_4snowdrop";
    snow=Readable(name,sizeof(prefix)) &&
      std::memcmp(base+name,prefix,sizeof(prefix)-1)==0;
  }
  __imp__sub_82593AE8(ctx,base);
  if(snow && Readable(object,1716)){
    // Generic snow is the continuous flake effect; snowdrop is Mansion's
    // intermittent falling clump effect. Resolve the shared loaded resource.
    auto call=ctx;call.r1.u32-=1024;
    const auto names=REX_LOAD_U32(0x82D6B2B0+72);
    const auto scratch=call.r1.u32+128;
    if(Readable(names,24)&&Readable(scratch,64)){
      constexpr char name[]="aid_xml_emitter_snow";
      std::memcpy(base+scratch,name,sizeof(name));
      call.r3.u64=names+20;call.r4.u64=scratch;call.r5.u64=0;
      sub_825E4798(call,base);
      if(Readable(call.r3.u32,436)){
        std::memcpy(base+object+1264,base+call.r3.u32,436);
        // 82603B20's camera-relative mode writes world coordinates into the
        // group's local offset. These groups already have an owner transform;
        // keep emission owner-relative after the cinematic camera hands off.
        base[object+1264+392]=0;
        Trace("Surface continuous snowflake resource resolved");
      }
    }
    for(unsigned i=0;i<16;++i)
      REX_STORE_U32(object+880+i*4,std::bit_cast<uint32_t>(i%5==0?1.f:0.f));
    surface_snow_emitters.push_back(object);
    Trace("Surface native snowfall emitter registered: "+std::to_string(object));
  }
}
REX_EXTERN(__imp__sub_82593ED8);
REX_HOOK_RAW(sub_82593ED8){
  std::erase(surface_snow_emitters,ctx.r3.u32);
  __imp__sub_82593ED8(ctx,base);
}
REX_EXTERN(__imp__sub_82594A40);
REX_HOOK_RAW(sub_82594A40){
  const auto object=ctx.r3.u32;
  const auto found=std::find(surface_snow_emitters.begin(),surface_snow_emitters.end(),object);
  if(test_background && SurfaceRequested(base) && found!=surface_snow_emitters.end() &&
     Readable(object,1716) && Readable(spawned_player,60)){
    const unsigned index=unsigned(found-surface_snow_emitters.begin());
    std::array<float,3> position{};
    bool valid=true;
    for(unsigned axis=0;axis<3;++axis){
      position[axis]=std::bit_cast<float>(REX_LOAD_U32(spawned_player+48+axis*4));
      valid=valid&&std::isfinite(position[axis]);
    }
    if(valid){
      for(auto& value:position)value=std::round(value/64.f)*64.f;
      position[0]+=(index&1)?240.f:-240.f;
      position[2]+=(index&2)?240.f:-240.f;
      position[1]+=450.f;
      bool moved=false;
      for(unsigned axis=0;axis<3;++axis){
        moved=moved||REX_LOAD_U32(object+48+axis*4)!=std::bit_cast<uint32_t>(position[axis]);
        REX_STORE_U32(object+928+axis*4,std::bit_cast<uint32_t>(position[axis]));
        REX_STORE_U32(object+48+axis*4,std::bit_cast<uint32_t>(position[axis]));
      }
      auto call=ctx;call.r1.u32-=1024;call.r3.u64=object;
      sub_825944A8(call,base);
      if(moved){
        if(REX_LOAD_U32(object+24)){
          call=ctx;call.r1.u32-=1024;call.r3.u64=object+16;
          sub_82504530(call,base);
        }
        call=ctx;call.r1.u32-=1024;call.r3.u64=object;
        sub_825082D0(call,base);
      }
      // Mansion alternates these emitters using mission-specific timers.
      // Surface weather is continuous; retain the native distance/budget checks.
      REX_STORE_U32(object+864,0);
      if(!REX_LOAD_U32(object+960)){
        call=ctx;call.r1.u32-=1024;call.r3.u64=object;
        sub_825946C0(call,base);
      }
      const auto particles=REX_LOAD_U32(object+960);
      if(Readable(particles,200) && REX_LOAD_U32(particles+184)==object){
        // Attached emitters add their owner's transform in 82604958.
        // Keep the particle offset local: a world-space offset doubles the
        // translation and sends the snowfall far outside Surface.
        for(unsigned axis=0;axis<3;++axis){
          REX_STORE_U32(particles+64+axis*4,0);
          REX_STORE_U32(particles+100+axis*4,std::bit_cast<uint32_t>(position[axis]));
        }
      }
    }
  }
  __imp__sub_82594A40(ctx,base);
}
REX_HOOK_RAW(sub_82702E08){
  uint32_t saved_texture=0,saved_light=0,device=0;
  std::array<uint32_t,2> saved_material{};
  std::array<uint32_t,12> saved_uv{};
  bool snow_bound=false;
  uint32_t atlas_slot=3;
  std::array<uint32_t,3> saved_normal_maps{};
  bool normal_maps_bound=false;
  if(test_background && ctx.r3.u32==test_background+1536){
    const auto colour_pass=REX_LOAD_U32(0x82E216B4);
    static uint32_t traced_passes=0;
    if(colour_pass<32 && !(traced_passes&(1u<<colour_pass))){
      traced_passes|=1u<<colour_pass;
      const auto active_device=REX_LOAD_U32(0x82E2A7B0);
      Trace("Surface material route: pass="+std::to_string(colour_pass)+
        " host="+std::to_string(REXCVAR_GET(pdz_surface_host_mission))+
        " device="+std::to_string(active_device)+
        " readable="+std::to_string(Readable(active_device,12716)));
    }
    // The complete trace confirms pass 1 is present; pass 2 is not the
    // colour pass and must retain its original deferred-render inputs.
    if(colour_pass==0){
      device=REX_LOAD_U32(0x82E2A7B0);
      const auto normal=SnowTexture(ctx,base,2);
      const auto material=SnowTexture(ctx,base,3);
      if(Readable(device,12724) && normal && material){
        auto call=ctx;call.r1.u32-=1024;
        for(unsigned i=0;i<3;++i){
          saved_normal_maps[i]=REX_LOAD_U32(device+12704+(i+2)*4);
          if(saved_normal_maps[i]){call.r3.u64=saved_normal_maps[i];sub_826D21A0(call,base);}
          call.r3.u64=device;call.r4.u64=i+2;call.r5.u64=i==2?material:normal;
          sub_826DD5B0(call,base);
        }
        normal_maps_bound=true;
        for(unsigned i=0;i<2;++i){
          const auto address=device+6016+16+8+i*4;
          saved_material[i]=REX_LOAD_U32(address);REX_STORE_U32(address,0);
        }
        REX_STORE_U64(device+24,REX_LOAD_U64(device+24)|(uint64_t(1)<<63));
      }
    }
    if(colour_pass==1 || colour_pass==7){
      // Pass 7 has a separate, simple diffuse shader using tf0. Leaving the
      // host texture there makes its material appear through the custom atlas.
      atlas_slot=colour_pass==7?0u:3u;
      device=REX_LOAD_U32(0x82E2A7B0);
      if(Readable(device,12720))if(const auto snow=SnowTexture(ctx,base)){
        saved_texture=REX_LOAD_U32(device+12704+atlas_slot*4);
        saved_light=REX_LOAD_U32(device+12704+2*4);
        auto call=ctx;call.r1.u32-=1024;
        if(saved_texture){call.r3.u64=saved_texture;sub_826D21A0(call,base);}
        if(saved_light){call.r3.u64=saved_light;sub_826D21A0(call,base);}
        call.r3.u64=device;call.r4.u64=atlas_slot;call.r5.u64=snow;
        sub_826DD5B0(call,base);snow_bound=true;
        if(const auto light=SnowTexture(ctx,base,true)){
          call.r3.u64=device;call.r4.u64=2;call.r5.u64=light;
          sub_826DD5B0(call,base);
        }
        // Mansion colour shader C127B524: tf3 is diffuse; tf2 is the
        // screen-space light buffer. Surface has no authored light volumes yet;
        // use neutral preview illumination and disable the borrowed parallax.
        for(unsigned i=0;i<2;++i){
          const auto address=device+6016+16+8+i*4;
          saved_material[i]=REX_LOAD_U32(address);
          REX_STORE_U32(address,0);
        }
        constexpr std::array<float,12> uv{1,0,0,0, 0,1,0,0, 0,0,1,0};
        for(unsigned i=0;i<uv.size();++i){
          saved_uv[i]=REX_LOAD_U32(device+1920+17*16+i*4);
          REX_STORE_U32(device+1920+17*16+i*4,std::bit_cast<uint32_t>(uv[i]));
        }
        REX_STORE_U64(device+24,REX_LOAD_U64(device+24)|(uint64_t(1)<<63));
        REX_STORE_U64(device+16,REX_LOAD_U64(device+16)|(uint64_t(1)<<59));
      }
    }
  }
  __imp__sub_82702E08(ctx,base);
  if(normal_maps_bound){
    auto call=ctx;call.r1.u32-=1024;
    for(unsigned i=0;i<3;++i){
      call.r3.u64=device;call.r4.u64=i+2;call.r5.u64=saved_normal_maps[i];
      sub_826DD5B0(call,base);
      if(saved_normal_maps[i]){call.r3.u64=saved_normal_maps[i];sub_826D2208(call,base);}
    }
    for(unsigned i=0;i<2;++i)REX_STORE_U32(device+6016+16+8+i*4,saved_material[i]);
    REX_STORE_U64(device+24,REX_LOAD_U64(device+24)|(uint64_t(1)<<63));
  }
  if(snow_bound){
    for(unsigned i=0;i<saved_material.size();++i)
      REX_STORE_U32(device+6016+16+8+i*4,saved_material[i]);
    for(unsigned i=0;i<saved_uv.size();++i)
      REX_STORE_U32(device+1920+17*16+i*4,saved_uv[i]);
    REX_STORE_U64(device+24,REX_LOAD_U64(device+24)|(uint64_t(1)<<63));
    REX_STORE_U64(device+16,REX_LOAD_U64(device+16)|(uint64_t(1)<<59));
    auto call=ctx;call.r1.u32-=1024;
    call.r3.u64=device;call.r4.u64=atlas_slot;call.r5.u64=saved_texture;
    sub_826DD5B0(call,base);
    if(saved_texture){call.r3.u64=saved_texture;sub_826D2208(call,base);}
    call.r3.u64=device;call.r4.u64=2;call.r5.u64=saved_light;
    sub_826DD5B0(call,base);
    if(saved_light){call.r3.u64=saved_light;sub_826D2208(call,base);}
  }
}





// Reuse two fully initialized native guards. Preserve their constructor's
// asset identity and registration; relocate via character physics in Tick.
REX_EXTERN(__imp__sub_8256FF40);
REX_HOOK_RAW(sub_8256FF40){
  const auto actor=ctx.r3.u32,source=ctx.r4.u32;
  int slot=-1;
  if(test_background && SurfaceRequested(base) &&
     REXCVAR_GET(pdz_surface_host_mission)==4 && Readable(source,268)){
    const auto name=REX_LOAD_U32(source+52);
    if(Readable(name,5)){
      if(std::memcmp(base+name,"fre1",5)==0)slot=0;
      if(std::memcmp(base+name,"wpt1",5)==0)slot=2;
      if(std::memcmp(base+name,"mgp1",5)==0)slot=3;
      if(std::memcmp(base+name,"tow1",5)==0)slot=4;
      if(std::memcmp(base+name,"tow2",5)==0)slot=5;
      if(std::memcmp(base+name,"tow3",5)==0)slot=6;
      else if(std::memcmp(base+name,"fre2",5)==0)slot=1;
    }
  }
  __imp__sub_8256FF40(ctx,base);
  pdz_coop::CharacterCreated(base,actor,source);
  pdz_population::Created(base,actor,source);
  if(slot>=0 && !cabin_guards[slot].actor){
    cabin_guards[slot]={actor,false};
    Trace("Surface cabin guard "+std::to_string(slot+1)+" registered: "+std::to_string(actor));
  }
}
REX_EXTERN(__imp__sub_82570688);
REX_HOOK_RAW(sub_82570688){
  const auto actor=ctx.r3.u32;
  __imp__sub_82570688(ctx,base);
  pdz_coop::CharacterDestroyed(actor);
  pdz_population::Destroyed(actor);
  for(auto& guard:cabin_guards)if(guard.actor==actor){
    guard={};
  }
}

// Read-only actor inventory for the cabin encounter adapter. This native
// entity callback is already reached by the host's ordinary update iterator.
REX_EXTERN(__imp__sub_8250D280);
REX_HOOK_RAW(sub_8250D280){
  const auto object=ctx.r3.u32;
  if(test_background && Readable(object,76) && surface_observed_entities.size()<512 &&
     surface_observed_entities.insert(object).second){
    Trace("Surface entity inventory: object="+std::to_string(object)+
      " class="+std::to_string(REX_LOAD_U32(object))+" type="+std::to_string(REX_LOAD_U32(object+12))+
      " model="+std::to_string(REX_LOAD_U32(object+72)));
  }
  __imp__sub_8250D280(ctx,base);
}


