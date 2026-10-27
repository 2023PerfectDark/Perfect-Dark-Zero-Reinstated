#include "generated/default/pdzstocktu3_init.h"
#include "mission_weather.h"
#include "cutscene_timing.h"
#include "native_cold_breath_data.h"
#include "native_rain_data.h"
#include "native_rain_impacts_data.h"
#include "mod/breathing_motion.h"
#include "mousehook.h"
#include "weather_audio.h"
#include "weather_areas.h"
#include <rex/runtime.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/hook.h>
REX_EXTERN(__imp__sub_82613498);
#include <rex/system/xmemory.h>
#include <imgui.h>
#include <rex/ui/image_decode.h>
#include <iterator>
#include <array>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <bit>
#include <fstream>
#include <string>
#include <cctype>
#include <atomic>
#include <cstring>

REXCVAR_DEFINE_BOOL(pdz_weather_enabled, true, "Gameplay", "Enable added weather, lens effects, splashes and weather audio");
REXCVAR_DEFINE_BOOL(pdz_custom_rain, true, "Gameplay", "Enable custom rain streaks, splashes and lens droplets in Missions 1 and 2");
REXCVAR_DEFINE_BOOL(pdz_custom_snow, true, "Gameplay", "Enable nearby custom snowflakes in addition to native snow");
namespace pdz_weather { namespace {
struct V {float x{},y{},z{};};
V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,float b){return {a.x*b,a.y*b,a.z*b};}
float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
bool Valid(uint32_t p,uint32_t n){
 auto* rt=rex::Runtime::instance();if(!rt||!p||uint64_t(p)+n>0x100000000ull)return false;
 for(uint64_t a=p;a<uint64_t(p)+n;){auto* h=rt->memory()->LookupHeap(uint32_t(a));uint32_t prot=0;
  if(!h||!h->QueryProtect(uint32_t(a),&prot)||!(prot&rex::memory::kMemoryProtectRead))return false;
  a=(a/h->page_size()+1)*h->page_size();}return true;
}
float F(uint8_t* base,uint32_t p){return std::bit_cast<float>(REX_LOAD_U32(p));}
V Read(uint8_t* base,uint32_t p){return {F(base,p),F(base,p+4),F(base,p+8)};}
// Recorded Mission 1 opening AudioScope shot. Match its camera station as
// well as its zoom: an unrelated cinematic close-up must not acquire droplets.
bool AudioScopeShot(uint8_t* base){
 if(!base||!pdz_cutscene::Active())return false;
 const auto session=REX_LOAD_U32(0x82D6BFD8),camera=REX_LOAD_U32(0x82D69048);
 if(!Valid(session,44)||REX_LOAD_U8(session+25)!=1||!Valid(camera,1092))return false;
 const auto mode=REX_LOAD_U32(camera+20),view=REX_LOAD_U32(camera+60);
 if(!Valid(mode,4)||REX_LOAD_U32(mode)!=0x82095EB0||!Valid(view,240))return false;
 const auto delta=Read(base,view+224)-V{-15395.f,-317.f,15557.f};
 const float fov=F(base,camera+1088);
 return fov>0.f&&fov<30.f&&Dot(delta,delta)<25.f*25.f;
}
void Write(uint8_t* base,uint32_t p,V v){REX_STORE_U32(p,std::bit_cast<uint32_t>(v.x));REX_STORE_U32(p+4,std::bit_cast<uint32_t>(v.y));REX_STORE_U32(p+8,std::bit_cast<uint32_t>(v.z));}
struct SurfaceHit {uint32_t prop=0,surface=0,material=0;V normal{};};
// All probes in one simulation tick share the same reserved stack region.
// Validate its pages once, rather than querying the heap for every ray.
thread_local uint32_t validated_probe_stack=0;
std::atomic<float> viewport_aspect{16.f/9.f};
bool Blocked(PPCContext call,uint8_t* base,uint32_t player,V a,V b,V* contact=nullptr,SurfaceHit* surface=nullptr){
 call.r1.u32-=2048;
 if(validated_probe_stack!=call.r1.u32){
  if(!Valid(call.r1.u32,2048))return true;
  validated_probe_stack=call.r1.u32;
 }
 auto from=call.r1.u32+256,to=from+16,hit=to+16;
 Write(base,from,a);Write(base,to,b);REX_STORE_U32(hit,0);
 for(auto off:{84,92,100})REX_STORE_U32(call.r1.u32+off,0);
 if(surface){REX_STORE_U32(hit+4,0);REX_STORE_U32(hit+8,0);Write(base,hit+32,{});REX_STORE_U32(call.r1.u32+84,hit+4);}
 call.r3.u64=player;call.r4.u64=from;call.r5.u64=to;call.r6.u64=4;
 call.r7.u64=hit;call.r8.u64=surface?hit+8:0;call.r9.u64=contact?hit+16:0;call.r10.u64=surface?hit+32:0;
 sub_82240620(call,base);if(contact&&call.r3.u8)*contact=Read(base,hit+16);
 if(surface&&call.r3.u8)*surface={REX_LOAD_U32(hit),REX_LOAD_U32(hit+4),REX_LOAD_U32(hit+8),Read(base,hit+32)};
 return call.r3.u8!=0;
}
std::string Text(uint8_t* base,uint32_t pointer){
 std::string out;for(unsigned i=0;i<160;++i){if(!Valid(pointer+i,1))break;const char c=char(REX_LOAD_U8(pointer+i));if(!c)break;if(c<32||c>126)return {};out+=char(std::tolower(static_cast<unsigned char>(c)));}return out;
}
std::string PropName(uint8_t* base,uint32_t prop){
 if(!Valid(prop,76)||REX_LOAD_U32(prop+12)!=11)return {};
 const auto model=REX_LOAD_U32(prop+72);if(!Valid(model,4))return {};
 const auto descriptor=REX_LOAD_U32(model);if(!Valid(descriptor,4))return {};
 return Text(base,REX_LOAD_U32(descriptor));
}
std::string SurfaceName(uint8_t* base,uint32_t id){
 const auto manager=REX_LOAD_U32(0x82D6C008);if(!Valid(manager,60))return {};
 const auto list=REX_LOAD_U32(manager+56);if(!Valid(list,12))return {};
 const auto table=REX_LOAD_U32(list),count=REX_LOAD_U32(list+8);
 if(count>256||!Valid(table,count*4))return {};
 for(uint32_t i=0;i<count;++i){const auto entry=REX_LOAD_U32(table+i*4);
  if(Valid(entry,136)&&REX_LOAD_U32(entry)==id)return Text(base,REX_LOAD_U32(entry+128));
 }return {};
}
// Mission 6 cave ceiling selected in-game on 2026-10-25. Resolve its
// collision surface afresh each session; never persist runtime pointers.
bool CaveRainCeiling(PPCContext ctx,uint8_t* base,uint32_t player,V eye,V* ceiling=nullptr){
 constexpr V reference{9844.802f,187.524f,4397.123f};
 SurfaceHit selected{};V referenceHit{};
 if(!Blocked(ctx,base,player,reference,reference+V{0,5000,0},&referenceHit,&selected)||
    (!selected.surface&&!selected.material)||selected.prop)return false;
 // Material IDs vary between cave mesh sections. Match the ceiling surface
 // category, and sample beside beams rather than treating each beam as an exit.
 bool match=false;V nearest{};float nearestHeight=5001.f;
 for(const V offset:std::array<V,5>{{{0,0,0},{120,0,0},{-120,0,0},{0,0,120},{0,0,-120}}}){
  SurfaceHit overhead{};V hit{};const V from=eye+offset;
  if(!Blocked(ctx,base,player,from,from+V{0,5000,0},&hit,&overhead))continue;
  if(overhead.prop||overhead.surface!=selected.surface)continue;
  match=true;
  if(hit.y-eye.y<nearestHeight){nearestHeight=hit.y-eye.y;nearest=hit;}
 }
 static uint32_t reported_surface=0,reported_material=0;
 if(selected.surface!=reported_surface||selected.material!=reported_material){
  REXLOG_INFO("Mission 6 cave rain reference: surface {:08X}, material {:08X}",selected.surface,selected.material);
  reported_surface=selected.surface;reported_material=selected.material;
 }
 if(match&&ceiling)*ceiling=nearest;
 return match;
}
RainSurface Classify(uint8_t* base,const SurfaceHit& hit){
 const auto name=PropName(base,hit.prop);
 auto has=[&](const char* s){return name.find(s)!=std::string::npos;};
 if(has("crate")||has("box_"))return Crate;
 if(has("vehicle")||has("truck")||has("taxi")||has("car_")||has("van_")||has("boat")||has("jeep"))return Vehicle;
 if(has("water")||has("pool")||has("pond"))return Water;
 if(has("metal")||has("barrel")||has("vent")||has("steel"))return Metal;
 const auto surface=SurfaceName(base,hit.surface);
 if(surface.find("water")!=std::string::npos||surface.find("puddle")!=std::string::npos)return Water;
 if(surface.find("metal")!=std::string::npos||surface.find("grate")!=std::string::npos)return Metal;
 return Ground;
}
RainAudio rain_audio;
struct AcousticPoint {V point{};RainSurface kind=Ground;float strength=0;std::chrono::steady_clock::time_point sampled{};};
std::array<AcousticPoint,12> acoustic{};
AcousticPoint gutter;
unsigned acoustic_cursor=0;
auto acoustic_next=std::chrono::steady_clock::time_point{};
float effects_volume=1;
std::array<float,SurfaceCount> acoustic_levels{};
uint32_t last_surface_id=0,last_material_id=0;
std::string last_surface_name,last_prop_name;
void Sound(PPCContext ctx,uint8_t* base,uint32_t player,V eye,V right,bool outside,unsigned level){
 const auto now=std::chrono::steady_clock::now();
 if(now>=acoustic_next){acoustic_next=now+std::chrono::milliseconds(125);
  const unsigned index=(acoustic_cursor++)%acoustic.size();
  const float angle=float(index)*6.2831853f/float(acoustic.size());
  const float radius=80.f+float(index%3)*110.f;
  const V offset{std::cos(angle)*radius,0,std::sin(angle)*radius};
  auto& sample=acoustic[index];sample.strength=0;
  V hit{};SurfaceHit info{};
  // Sample nearby landing surfaces only when they are exposed to rain and
  // audible from the listener. Unknown materials conservatively use ground.
  const V origin=eye+offset+V{0,350,0};
  if(Blocked(ctx,base,player,origin,origin+V{0,-1000,0},&hit,&info)){
   const V above=hit+V{0,4,0};
   if(!Blocked(ctx,base,player,above,above+V{0,5000,0})&&!Blocked(ctx,base,player,eye,above)){
    sample={above,Classify(base,info),1.f,now};last_surface_id=info.surface;last_material_id=info.material;
    last_surface_name=SurfaceName(base,info.surface);last_prop_name=PropName(base,info.prop);
   }
  }
  // One rotating exterior-wall probe. Require sky exposure outside the wall
  // plus a solid roof above its inside edge, avoiding arbitrary indoor walls.
  const V direction{std::cos(angle),0,std::sin(angle)};
  if(Blocked(ctx,base,player,eye,eye+direction*650.f,&hit,&info)&&!info.prop&&std::abs(info.normal.y)<.4f){
   const V exterior=hit-direction*15.f;
   if(!Blocked(ctx,base,player,exterior,exterior+V{0,5000,0})&&Blocked(ctx,base,player,hit+direction*25.f,hit+direction*25.f+V{0,1000,0}))gutter={exterior,Gutter,1.f,now};
  }
  gutter.strength=std::max(0.f,gutter.strength-.04f);
  // Native effects-category gain, including the player's sound preference.
  auto volume=ctx;volume.r1.u32-=512;
  if(Valid(volume.r1.u32-16,528)){
   volume.r3.u64=13;volume.r4.u64=REX_LOAD_U8(0x82E67C48+13*32+28);sub_821FC158(volume,base);
   effects_volume=std::clamp(float(volume.r3.u32)/255.f,0.f,1.f);
  }
  std::array<float,SurfaceCount> levels{},pans{},weights{};
  auto add=[&](const AcousticPoint& p){const V delta=p.point-eye;const float distance=std::sqrt(Dot(delta,delta));
   if(now-p.sampled>std::chrono::milliseconds(1600))return;
   const float range=p.kind==Gutter?700.f:400.f;
   const float falloff=std::clamp(1.f-distance/range,0.f,1.f);
   const float weight=p.strength*falloff*falloff;
   levels[p.kind]+=weight*.25f;pans[p.kind]+=weight*Dot(delta,right)/std::max(1.f,distance);weights[p.kind]+=weight;
  };
  for(const auto& p:acoustic)add(p);add(gutter);
  // No global rain bed: every audible layer must have an exposed surface hit.
  for(unsigned i=0;i<SurfaceCount;++i){pans[i]=weights[i]>0?pans[i]/weights[i]:0;levels[i]=std::min(.7f,levels[i])*(level==1?.6f:1.f);}
  acoustic_levels=levels;
  rain_audio.Update(levels,pans,effects_volume);
 }
}
struct Particle {V p{},impact{},velocity{};float size=3.f;float floor=-100000.f;bool alive=false,visible=false,has_surface=false;};
struct Splash {V p{};float age=1.f,seed=0;bool visible=false;V normal{0,1,0};};
struct Streak {float x,y,tx,ty,alpha;float width=1.f;};
struct Frame {bool active=false,outdoors=false,cinematic=false;float upward=0;unsigned mission=0;std::vector<Streak> lines,splashes,snow;};
float breath_cycle=0,previous_breath_phase=0,previous_breath_health=0;
uint32_t breath_health_owner=0;
V breath_mouth{},breath_direction{};
bool breath_tracking=false;
breathing_motion::State breath_fallback;
float last_camera_breath_phase=-1.f,breath_clock_idle=0;
V last_breath_position{};
bool breath_position_valid=false;
uint32_t breath_definition=0,breath_copy=0;
bool breath_reported=false,breath_attempted=false;
void ResetBreath(){breath_cycle=0;breath_tracking=false;breath_fallback={};
 last_camera_breath_phase=-1.f;breath_clock_idle=0;breath_position_valid=false;}
// Native emitter definitions are 436 bytes (including resolved texture and
// child-emitter pointers at +420..432). Keep one owned copy for gameplay-only
// visibility changes, leaving the original cutscene definition untouched.
// Resolve the animated head joint through the same native transform used for
// character head targeting. Camera eye points can sit at torso height in cover.
bool BreathHead(PPCContext call,uint8_t* base,uint32_t actor,V& head){
 if(!Valid(actor,76))return false;
 const auto model=REX_LOAD_U32(actor+72);if(!Valid(model,80))return false;
 const auto def=REX_LOAD_U32(model);if(!Valid(def,16))return false;
 const auto skeleton=REX_LOAD_U32(def+12);if(!Valid(skeleton,56))return false;
 const auto count=REX_LOAD_U32(skeleton+48),index=REX_LOAD_U32(0x82FBE3C0);
 if(count<=17||count>256||index>3||!REX_LOAD_U32(model+60)||
    !Valid(REX_LOAD_U32(skeleton+52),count*36)||
    !Valid(REX_LOAD_U32(model+60+index*4),count*64)||
    !Valid(REX_LOAD_U32(model+40+index*4),64))return false;
 call.r1.u32-=2048;if(!Valid(call.r1.u32,2048))return false;
 const auto stack=call.r1.u32;
 call.r3.u64=model;call.r4.u64=17;call.r5.u64=stack+256;call.r6.u64=stack+320;
 sub_82413B18(call,base);
 head=Read(base,stack+368);
 return std::isfinite(Dot(head,head))&&Dot(head-Read(base,actor+48),head-Read(base,actor+48))<300.f*300.f;
}
bool NativeBreath(PPCContext call,uint8_t* base,V mouth,V direction){
 call.r1.u32-=4096;if(!Valid(call.r1.u32,4096))return false;
 const auto stack=call.r1.u32;
 if(!breath_definition){
  if(breath_attempted)return false;
  breath_attempted=true;
  const auto manager=REX_LOAD_U32(0x82D6BFD8+72);
  if(!Valid(manager,24))return false;
  constexpr char name[]="aid_xml_emitter_cut_04coldbreath";
  std::memcpy(base+stack+512,name,sizeof(name));
  // Same definition/texture resolver used by the native particle preload.
  call.r3.u64=manager+20;call.r4.u64=stack+512;call.r5.u64=0;
  sub_825F41B0(call,base);
  const auto definition=call.r3.u32;
  if(!breath_copy)breath_copy=rex::Runtime::instance()->memory()->SystemHeapAlloc(sizeof(native_cold_breath_data),16);
  if(!Valid(breath_copy,sizeof(native_cold_breath_data)))return false;
  if(Valid(definition,436))std::memcpy(base+breath_copy,base+definition,436);
  else{
   // Mission 6 does not preload the cutscene definition. Resolve its common
   // smoke textures against the current mission, never reuse Mission 4 pointers.
   std::memcpy(base+breath_copy,native_cold_breath_data,sizeof(native_cold_breath_data));
   for(unsigned offset:{300u,320u,348u,352u,356u})
    REX_STORE_U32(breath_copy+offset,breath_copy+REX_LOAD_U32(breath_copy+offset));
   for(auto pair:std::array<std::pair<unsigned,unsigned>,3>{{{352,420},{320,424},{356,428}}}){
    call.r3.u64=REX_LOAD_U32(breath_copy+pair.first);sub_82610888(call,base);
    REX_STORE_U32(breath_copy+pair.second,call.r3.u32);
   }
   if(!REX_LOAD_U32(breath_copy+420)){
    REXLOG_WARN("Native cold breath: smoke texture unavailable in this mission");return false;
   }
   call.r3.u64=manager+20;call.r4.u64=breath_copy;sub_825F3E78(call,base);
   REXLOG_INFO("Native cold breath: standalone definition resolved for current mission");
  }
  REX_STORE_U8(breath_copy+360,0); // Exhaled particles remain in world space.
  REX_STORE_U8(breath_copy+416,0); // Allow gameplay as well as cutscene mode.
  breath_definition=breath_copy;
 }
 Write(base,stack+256,mouth);Write(base,stack+272,direction);
 // Full native emitter constructor. World-space origin/orientation, no owner
 // or third-person-only flag, finite emission, normal visibility culling.
 std::memset(base+stack+80,0,40);
 REX_STORE_U32(stack+92,0xFFFFFFFF);REX_STORE_U8(stack+111,1);
 call.r3.u64=0;call.r4.u64=breath_definition;call.r5.u64=stack+256;
 call.r6.u64=stack+272;call.r7.u64=0;call.r8.u64=0;call.r9.u64=0;call.r10.u64=1;
 call.f1.f64=.6;
 sub_82611C98(call,base);
 const bool emitted=call.r3.u32!=0;
 // Native particle creation multiplies colour alpha by emitter +176.
 if(emitted&&Valid(call.r3.u32,180)){
  REX_STORE_U32(call.r3.u32+176,std::bit_cast<uint32_t>(.20f));
  // Native spawn scales both particle size endpoints by emitter +120.
  REX_STORE_U32(call.r3.u32+120,std::bit_cast<uint32_t>(1.5f));
 }
 if(emitted&&!breath_reported){breath_reported=true;REXLOG_INFO("Native cold breath emitted: {:08X}",call.r3.u32);}
 return emitted;
}
std::mutex frame_mutex;Frame frame;
uint32_t rain_definition=0,rain_copy=0,rain_instance=0;
uint32_t splash_definition=0,splash_copy=0;
unsigned native_impact_count=0;
auto native_impact_next=std::chrono::steady_clock::time_point{};
uint32_t LoadRainImpact(PPCContext call,uint8_t* base,uint32_t& copy,const uint8_t* data,unsigned size){
 if(!copy)copy=rex::Runtime::instance()->memory()->SystemHeapAlloc(size,16);
 if(!Valid(copy,size))return 0;
 std::memcpy(base+copy,data,size);
 for(unsigned o:{300u,320u,348u,352u,356u})REX_STORE_U32(copy+o,copy+REX_LOAD_U32(copy+o));
 for(auto pair:std::array<std::pair<unsigned,unsigned>,3>{{{352,420},{320,424},{356,428}}}){
  call.r3.u64=REX_LOAD_U32(copy+pair.first);sub_82610888(call,base);REX_STORE_U32(copy+pair.second,call.r3.u32);
 }
 if(!REX_LOAD_U32(copy+420))return 0;
 auto set=[&](unsigned o,float v){REX_STORE_U32(copy+o,std::bit_cast<uint32_t>(v));};
 set(8,0);set(12,0);set(16,0);
 // Both endpoints participate in native size interpolation. Leaving the
 // asset's 70-100 end size made the small initial splash grow dramatically.
 set(268,6.f);set(272,6.f);set(276,10.f);set(280,10.f);
 set(364,.12f);set(368,40.f);
 REX_STORE_U8(copy+416,0);
 REXLOG_INFO("Native rain: splash ready {:08X}, texture {:08X}",copy,REX_LOAD_U32(copy+420));
 return copy;
}
void NativeRainImpact(PPCContext call,uint8_t* base,uint32_t player,V eye,V forward,bool cave=false){
 const auto now=std::chrono::steady_clock::now();
 if(now<native_impact_next||!splash_definition)return;
 native_impact_next=now+std::chrono::milliseconds(400);
 // Use the native collision query to place short-lived native emitters on
 // exposed surfaces, rather than drawing screen-space splash approximations.
 static std::minstd_rand rng{7351};
 auto random=[&](){return float(rng()%10000)/10000.f;};
 V flat{forward.x,0,forward.z};const float len=std::sqrt(Dot(flat,flat));
 if(len>.01f)flat=flat*(1.f/len);
 const V side{-flat.z,0,flat.x};
 const float downward=std::clamp(-forward.y,0.f,1.f);
 const V start=eye+flat*((1.f-downward)*(40.f+random()*260.f))+side*((random()-.5f)*240.f)+V{0,cave?0.f:300.f,0};
 V point{};SurfaceHit info{};
 if(!Blocked(call,base,player,start,start+V{0,-1100,0},&point,&info)||info.normal.y<.45f)return;
 point=point+info.normal*2.f;
 if(cave){if(!CaveRainCeiling(call,base,player,point))return;}
 else if(Blocked(call,base,player,point,point+V{0,5000,0}))return;
 call.r1.u32-=4096;if(!Valid(call.r1.u32,4096))return;
 const auto stack=call.r1.u32;
 Write(base,stack+256,point);Write(base,stack+272,info.normal);
 const auto definition=splash_definition;
 std::memset(base+stack+80,0,40);REX_STORE_U32(stack+92,0xFFFFFFFF);REX_STORE_U8(stack+111,1);
 call.r3.u64=0;call.r4.u64=definition;call.r5.u64=stack+256;call.r6.u64=stack+272;
 call.r7.u64=0;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;call.f1.f64=F(base,definition+364);
 sub_82611C98(call,base);
 // A one-shot must emit at least once even when its short duration ends
 // before the native rate accumulator would reach a whole particle.
 if(call.r3.u32&&Valid(call.r3.u32,132))REX_STORE_U32(call.r3.u32+128,std::bit_cast<uint32_t>(1.f));
 if(call.r3.u32&&native_impact_count++<4)REXLOG_INFO("Native rain: splash impact spawned {:08X}",call.r3.u32);
}
bool rain_attempted=false;
void StopNativeRain(PPCContext call,uint8_t* base){
 if(rain_instance&&Valid(rain_instance,180)&&REX_LOAD_U32(rain_instance+156)==rain_definition){
  call.r1.u32-=4096;
  if(Valid(call.r1.u32,4096)){
   call.r3.u64=rain_instance;sub_82613260(call,base);
   call.r3.u64=rain_instance;sub_82613398(call,base);
  }
 }
 rain_instance=0;
}
bool NativeRain(PPCContext call,uint8_t* base,V eye,bool exposed,unsigned level,float dt,float height=500.f){
 call.r1.u32-=4096;if(!Valid(call.r1.u32,4096))return false;
 const auto stack=call.r1.u32;
 if(!rain_definition&&!rain_attempted){
  rain_attempted=true;
  if(!rain_copy)rain_copy=rex::Runtime::instance()->memory()->SystemHeapAlloc(sizeof(native_rain_data),16);
  if(!Valid(rain_copy,sizeof(native_rain_data)))return false;
  std::memcpy(base+rain_copy,native_rain_data,sizeof(native_rain_data));
  for(unsigned o:{300u,320u,348u,352u,356u})REX_STORE_U32(rain_copy+o,rain_copy+REX_LOAD_U32(rain_copy+o));
  for(auto pair:std::array<std::pair<unsigned,unsigned>,3>{{{352,420},{320,424},{356,428}}}){
   call.r3.u64=REX_LOAD_U32(rain_copy+pair.first);sub_82610888(call,base);
   REX_STORE_U32(rain_copy+pair.second,call.r3.u32);
  }
  if(!REX_LOAD_U32(rain_copy+420)){REXLOG_WARN("Native rain: waterdrop texture unavailable");return false;}
  auto set=[&](unsigned o,float v){REX_STORE_U32(rain_copy+o,std::bit_cast<uint32_t>(v));};
  // Sparse cave seepage using the original waterdrop scale. A small initial
  // downward speed lets native gravity accelerate each drop from the roof.
  set(8,450.f);set(16,450.f);set(368,24.f);
  set(268,2.f);set(272,4.f);
  set(40,80.f);set(52,160.f);
  set(292,1.8f);set(296,1.8f);
  REX_STORE_U8(rain_copy+360,0);REX_STORE_U8(rain_copy+392,0);REX_STORE_U8(rain_copy+416,0);
  rain_definition=rain_copy;
  // Added cave splashes are suspended while isolating the Mission 6 combat
  // particle regression. Do not load their heat-haze material or spawn them.
  // Keep definitions private: authored effects own the weather globals.
  REXLOG_INFO("Native rain: waterdrop definition ready {:08X}, texture {:08X}",rain_definition,REX_LOAD_U32(rain_copy+420));
 }
 if(!rain_definition)return false;
 if(rain_instance&&(!Valid(rain_instance,180)||REX_LOAD_U32(rain_instance+156)!=rain_definition))rain_instance=0;
 if(!exposed){
  StopNativeRain(call,base);
  return true;
 }
 const V origin=eye+V{0,height,0};
 if(!rain_instance){
  Write(base,stack+256,origin);Write(base,stack+272,{0,-1,0});
  std::memset(base+stack+80,0,40);REX_STORE_U32(stack+92,0xFFFFFFFF);REX_STORE_U8(stack+111,1);
  call.r3.u64=0;call.r4.u64=rain_definition;call.r5.u64=stack+256;call.r6.u64=stack+272;
  // The overhead volume can lie outside the player's room; individual native
  // particles still render normally, but origin-only visibility rejects it.
  call.r7.u64=0;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;call.f1.f64=-1;
  sub_82611C98(call,base);rain_instance=call.r3.u32;
  if(rain_instance){
   REXLOG_INFO("Native rain: started {:08X}",rain_instance);
  }
 }
 if(rain_instance){Write(base,rain_instance+64,origin);auto update=call;update.r3.u64=rain_instance;sub_82614480(update,base);}
 // Native scheduling already updates this standalone emitter. Do not start
 // the global weather/lens system or advance it a second time from this hook.
 if(rain_instance&&Valid(rain_instance,116)){
  const bool full=REX_LOAD_U32(rain_instance+88)>=2;
  REX_STORE_U32(rain_instance+112,std::bit_cast<uint32_t>(full?0.f:1.f));
 }
 return rain_instance!=0;
}
std::array<Particle,1080> particles{};
std::array<Splash,48> splashes{};
unsigned splash_cursor=0,splash_visibility=0;
unsigned mission=0,cursor=0,snow_collision_cursor=0;
uint32_t added_native_snow=0;
std::mt19937 random{0x5241494e};
float Random(float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(random);}
auto last=std::chrono::steady_clock::time_point{};
auto roof_next=last,report_next=last;
bool outdoors=false;
bool cave_rain=false;
float cave_rain_height=500.f;
void Publish(Frame value){std::lock_guard lock(frame_mutex);frame=std::move(value);}
}
void Tick(PPCContext& ctx,uint8_t* base){
 validated_probe_stack=0;
 if(!REXCVAR_GET(pdz_weather_enabled)){StopNativeRain(ctx,base);rain_audio.Pause();Publish({});last={};mission=0;ResetBreath();return;}
 const auto session=REX_LOAD_U32(0x82D6BFD8);
 if(!pdz_input::Supported()||!Valid(session,44)){StopNativeRain(ctx,base);rain_audio.Pause();Publish({});last={};return;}
 const unsigned level=REX_LOAD_U8(session+25);
 if((level!=1&&level!=2&&level!=4&&level!=6&&level!=7)||REX_LOAD_U32(session+40)>1){StopNativeRain(ctx,base);rain_audio.Pause();Publish({});last={};mission=0;added_native_snow=0;return;}
 if(mission!=level){added_native_snow=0;rain_definition=0;rain_instance=0;rain_attempted=false;splash_definition=0;native_impact_count=0;native_impact_next={};}
 if(mission!=level){mission=level;particles={};splashes={};ResetBreath();breath_definition=0;breath_attempted=false;breath_reported=false;cursor=0;splash_cursor=0;splash_visibility=0;roof_next={};outdoors=false;acoustic={};gutter={};acoustic_next={};acoustic_cursor=0;rain_audio.Pause();}
 const bool cinematicSnow=(level==4||level==6||level==7)&&pdz_cutscene::Active()&&!REX_LOAD_U8(session);
 const bool cinematicRain=level==1&&pdz_cutscene::Active();
 if(!pdz_input::WeatherVisible(cinematicRain)&&!cinematicSnow){StopNativeRain(ctx,base);rain_audio.Pause();Publish({});last={};ResetBreath();return;}
 const auto camera=REX_LOAD_U32(0x82D69048);
 if(!Valid(camera,1092)){rain_audio.Pause();Publish({});last={};return;}
 const auto cameraPlayer=REX_LOAD_U32(camera+8),view=REX_LOAD_U32(camera+60);
 const auto player=Valid(cameraPlayer,60)?cameraPlayer:0;
 if((!cinematicSnow&&!cinematicRain&&!player)||!Valid(view,240)){rain_audio.Pause();Publish({});last={};return;}
 const auto now=std::chrono::steady_clock::now();
 float dt=last.time_since_epoch().count()?float(std::chrono::duration<double>(now-last).count()):0.f;
 if(dt>0&&dt<1.f/30.f)return;last=now;dt=std::clamp(dt,0.f,.1f);
 const V eye=Read(base,view+224),right=Read(base,view+176),up=Read(base,view+192),forward=Read(base,view+208)*-1.f;
 const float fov=F(base,camera+1088);
 if(!std::isfinite(Dot(eye,eye))||!std::isfinite(fov)||fov<.1f||fov>175){rain_audio.Pause();Publish({});return;}
 const float tangent=std::tan(fov*3.14159265f/360.f);
 if(cinematicRain||now>=roof_next){
  outdoors=!Blocked(ctx,base,player,eye,eye+V{0,5000,0});
  V ceiling{};const bool wasCaveRain=cave_rain;
  cave_rain=level==6&&CaveRainCeiling(ctx,base,player,eye,&ceiling);
  if(cave_rain!=wasCaveRain)REXLOG_INFO("Mission 6 cave rain: {}",cave_rain?"started":"stopped");
  cave_rain_height=cave_rain?std::clamp(ceiling.y-eye.y-20.f,10.f,500.f):500.f;
  roof_next=now+std::chrono::milliseconds(300);
 }
 // Evaluate the cinematic camera, not Joanna's gameplay position. Clear the
 // particle snapshot immediately on an indoor shot so a camera cut cannot
 // carry the previous outdoor rain across the transition.
 if(cinematicRain&&!outdoors){particles={};splashes={};rain_audio.Pause();Publish({});return;}
 const bool snow=level==4||level==6||level==7;
 // The normal ray excludes Joanna to keep listener/roof probes usable. Only
 // falling rain in cover includes her native collision geometry (ignore=0).
 const uint32_t mode=Valid(camera,0x540)?REX_LOAD_U32(camera+0x53C):0;
 const bool cover=!cinematicRain&&!snow&&Valid(mode,12)&&REX_LOAD_U8(mode+11)==3;
 const V joanna=Valid(player,60)?Read(base,player+48):eye;
 // Missions 1 and 2 use the custom weather renderer again.
 const bool nativeRain=level==6&&NativeRain(ctx,base,eye,cave_rain,level,dt,cave_rain_height);
 if(level!=6)StopNativeRain(ctx,base);
 if(snow)rain_audio.Pause();else Sound(ctx,base,player,eye,right,outdoors,level);
 if(!snow&&!REXCVAR_GET(pdz_custom_rain)){
  particles={};splashes={};Publish({});return;
 }
 // Use the native continuous snow effect in each requested snowy mission.
 // Track only our added instance, so authored effects remain game-controlled.
 if(snow){
  constexpr uint32_t nativeSnow=uint32_t(-32004*65536ll-11816+4);
  constexpr uint32_t snowDefinition=uint32_t(-32030*65536ll-17756);
  const auto instance=REX_LOAD_U32(nativeSnow);
 const auto localWeather=pdz_weather_areas::At(level,{joanna.x,joanna.y,joanna.z});
 const bool snowExposed=!cave_rain&&(outdoors||localWeather.snow);
 const bool start=snowExposed&&!instance&&Valid(REX_LOAD_U32(snowDefinition),436);
 const bool stop=!snowExposed&&instance&&instance==added_native_snow;
  if(start||stop){
   auto call=ctx;call.r1.u32-=1024;
   if(Valid(call.r1.u32,1024)){
    call.f1.f64=start?1.0:0.0;sub_825F6148(call,base);
    added_native_snow=start?REX_LOAD_U32(nativeSnow):0;
    REXLOG_INFO("Native snow mission {}: {} instance {:08X}",level,start?"started":"stopped",added_native_snow);
   }
  }
  // Native emitter +120 scales both particle size endpoints at spawn.
  const auto snowfall=REX_LOAD_U32(nativeSnow);
  if(Valid(snowfall,124)){
   REX_STORE_U32(snowfall+120,std::bit_cast<uint32_t>(1.5f));
   // Native update multiplies emission accumulation by emitter +112.
   REX_STORE_U32(snowfall+112,std::bit_cast<uint32_t>(level==7?.75f:1.5f));
  }
 }
 if(cinematicSnow){rain_audio.Pause();Publish({});last={};ResetBreath();return;}
 const unsigned count=level==6||level==7||(snow&&!REXCVAR_GET(pdz_custom_snow))?0:snow?720:level==1?80:240;
 // Shared slow gusts keep adjacent drops moving as one weather field.
 static float rain_time=0;rain_time+=dt;
 const float gust=std::sin(rain_time*.43f)*.65f+std::sin(rain_time*.17f)*.35f;
 const V velocity{45.f+65.f*gust,-(level==1?1500.f:2100.f),15.f+30.f*std::sin(rain_time*.31f)};
 const unsigned snow_probe_start=snow_collision_cursor;
 if(snow&&count)snow_collision_cursor=(snow_collision_cursor+64)%count;
 for(auto& s:splashes)s.age+=dt;
 for(unsigned i=0;i<count;++i){auto& p=particles[i];if(!p.alive)continue;
  if(!snow){const float follow=1.f-std::exp(-dt*1.4f);
   p.velocity.x+=(velocity.x-p.velocity.x)*follow;
   p.velocity.z+=(velocity.z-p.velocity.z)*follow;}
  V next=p.p+p.velocity*dt,contact{};SurfaceHit surface{};
  // Sweep the actual travelled segment, including moving doors. A floor
  // sampled at birth cannot protect against a door closing across the path.
  const bool snow_probe=snow&&((i+count-snow_probe_start)%count)<64;
  // Sweep from the last checked position: staggered checks must not skip walls.
  const bool hit=dt>0&&(!snow||snow_probe)&&Blocked(ctx,base,cover?0:player,
    snow?p.impact:p.p,next,&contact,&surface);
  if(snow_probe){
   p.impact=next;
   if(hit)p.alive=false;
   p.visible=!hit&&!Blocked(ctx,base,player,eye,next);
  }
  if(hit&&!snow){
   // Only real collision contacts create splashes, never the range cutoff.
   auto& s=splashes[(splash_cursor++)%splashes.size()];
   s={contact+surface.normal*2.f,0.f,Random(0,6.283185f),false};
   const float normal_length=std::sqrt(Dot(surface.normal,surface.normal));
   if(std::isfinite(normal_length)&&normal_length>.01f)s.normal=surface.normal*(1.f/normal_length);
   if(Dot(s.normal,p.velocity)>0)s.normal=s.normal*-1.f;
   s.p=contact+s.normal*1.5f;
   p.alive=false;
  }
  p.p=next;
  if(p.p.y<eye.y-500.f||Dot(p.p-eye,p.p-eye)>(level==6?360000.f:3600000.f))p.alive=false;
 }
 // Bound geometry queries independently of the renderer's frame rate. Native
 // collision rejects indoor emitters and rain hidden behind solid geometry.
 for(unsigned n=0;count&&n<(level==6?8u:snow?24u:12u);++n){auto& p=particles[(cursor++)%count];
  if(!p.alive){p.p=eye+V{Random(-1350,1350),Random(100,1000),Random(-1350,1350)};
   if(level==6)p.p=eye+V{Random(-350,350),Random(100,400),Random(-350,350)};
   // Reserve part of the existing budget for close drops. They still travel
   // through the native collision query; roofs/walls win before body hits.
   if(cover&&n<3&&std::isfinite(Dot(joanna,joanna)))
     p.p=joanna+V{Random(-38,22),Random(220,340),Random(-30,30)};
   p.velocity=snow?V{Random(45.f,115.f),-Random(110.f,240.f),Random(-45.f,45.f)}:
     V{velocity.x+Random(-18.f,18.f),velocity.y*Random(.8f,1.2f),velocity.z+Random(-12.f,12.f)};
   p.size=snow?Random(2.f,5.f):Random(.8f,2.6f);
   p.alive=(!snow||outdoors)&&!Blocked(ctx,base,player,p.p,p.p+V{0,5000,0});p.visible=false;p.impact=p.p;
  }
 }
 Frame output;output.active=true;output.outdoors=outdoors;output.mission=level;output.cinematic=cinematicRain;
 output.snow.reserve(snow?count:0);
 output.upward=std::clamp(forward.y,-1.f,1.f);
 // Fixed budget for impact occlusion; no per-render-frame native queries.
 for(unsigned n=0;n<8;++n){auto& s=splashes[(splash_visibility++)%splashes.size()];
  if(s.age<.32f){
   const V toward=eye-s.p;const float distance=std::sqrt(Dot(toward,toward));
   s.visible=distance>1.f&&!Blocked(ctx,base,player,eye,s.p+toward*(2.f/distance));
  }
 }
 auto project=[&](V p,float& x,float& y){
  const V a=p-eye;const float depth=Dot(a,forward);
  if(depth<35.f)return false;
  x=Dot(a,right)/(depth*tangent);y=-Dot(a,up)/(depth*tangent);
  return std::abs(x)<3.f&&std::abs(y)<1.2f;
 };
 // Native gameplay mode eye/look points follow Joanna in both views.
 constexpr uint32_t breath_camera_vtable=0x82095EF0;
 const uint32_t breath_mode=REX_LOAD_U32(camera+0x14); // Native gameplay camera controller.
 if((level==4||level==6||level==7)&&Valid(breath_mode,200)&&REX_LOAD_U32(breath_mode)==breath_camera_vtable){
  const V head=Read(base,breath_mode+188),look=Read(base,breath_mode+16)-head;
  const float length=std::sqrt(Dot(look,look));
  const bool head_valid=std::isfinite(Dot(head,head))&&std::isfinite(length)&&length>1.f&&
    Dot(head-joanna,head-joanna)<1000.f*1000.f;
  if(Valid(player,6564)){
   const auto health=REX_LOAD_U32(player+6560);
   if(Valid(health,984)){
    const float hp=F(base,health+980);
    if(std::isfinite(hp)){
     if(breath_health_owner==player&&hp<previous_breath_health-.01f)
      breathing_motion::damageImpulse.store(.65f);
     previous_breath_health=hp;breath_health_owner=player;
    }
   }
  }
  const float camera_phase=breathing_motion::visiblePhase.load();
  const V movement=joanna-last_breath_position;
  const float distance=std::sqrt(movement.x*movement.x+movement.z*movement.z);
  const float speed=breath_position_valid&&dt>0&&distance<100.f?distance/dt:0.f;
  last_breath_position=joanna;breath_position_valid=true;
  if(camera_phase!=last_camera_breath_phase){
   // Resume synchronizing with the head bob whenever its clock advances.
   if(breath_clock_idle>.25f)previous_breath_phase=camera_phase;
   last_camera_breath_phase=camera_phase;breath_clock_idle=0;
   breath_fallback.phase=camera_phase;
   breath_fallback.exertion=breathing_motion::visibleExertion.load();
  }else breath_clock_idle+=dt;
  // Native camera updates can stop advancing the head-bob clock in third
  // person. Keep the same stamina cycle alive without publishing back into it.
  if(breath_clock_idle>.25f)breath_fallback.Step(dt,speed,true,false,false,0,0,false,false);
  const float phase=breath_clock_idle>.25f?float(breath_fallback.phase):camera_phase;
  const bool exhale=phase<previous_breath_phase;
  previous_breath_phase=phase;
  if(head_valid){
   if(outdoors||pdz_weather_areas::At(level,{joanna.x,joanna.y,joanna.z}).breath){
    V direction=look*(1.f/length),mouth{};
    const bool first_person=Dot(eye-head,eye-head)<35.f*35.f;
    if(first_person){
     // Keep the cloud beyond the first-person near plane, just below the
     // view centre. A world-down offset disappears below the view when level.
     direction=forward*(1.f/std::sqrt(Dot(forward,forward)));
     // Match apparent cloud size across FOVs, bounded by the near plane.
     const float depth=std::max(40.f,55.f*std::tan(104.f*3.14159265f/360.f)/tangent);
     // NDC y=0.65 places emission at 82.5% of viewport height.
     mouth=eye+direction*depth-up*(depth*tangent*.65f);
    }else{
     V animated_head{};
     if(!BreathHead(ctx,base,player,animated_head)){breath_cycle=.3f;return;}
     mouth=animated_head+direction*6.f+V{0,-3.f,0};
    }
    breath_mouth=mouth;breath_direction=direction;breath_tracking=true;
    if(exhale)NativeBreath(ctx,base,mouth,direction);
   }else breath_cycle=.3f;
  }
 }else ResetBreath();
 for(const auto& s:splashes){
  if(s.age>=.32f||!s.visible)continue;
  const float alpha=(1.f-s.age/.32f)*std::clamp(1.f-std::sqrt(Dot(s.p-eye,s.p-eye))/1800.f,0.f,1.f);
  // Build a local surface frame: wall hits spray away from the wall, while
  // floor hits form small irregular crowns instead of identical four-way stars.
  const V reference=std::abs(s.normal.y)<.9f?V{0,1,0}:V{1,0,0};
  V tangent_axis=reference-s.normal*Dot(reference,s.normal);
  tangent_axis=tangent_axis*(1.f/std::sqrt(Dot(tangent_axis,tangent_axis)));
  const V bitangent{s.normal.y*tangent_axis.z-s.normal.z*tangent_axis.y,
    s.normal.z*tangent_axis.x-s.normal.x*tangent_axis.z,
    s.normal.x*tangent_axis.y-s.normal.y*tangent_axis.x};
  for(unsigned n=0;n<7;++n){
   const float variation=.5f+.5f*std::sin(s.seed*7.3f+n*9.17f);
   const float angle=s.seed+n*.897598f+variation*.35f;
   const V lateral=tangent_axis*std::cos(angle)+bitangent*std::sin(angle);
   auto spray=[&](float t){return s.p+lateral*((17.f+variation*31.f)*t)+
     s.normal*((46.f+variation*50.f)*t)+V{0,-280.f*t*t,0};};
   const V a=spray(s.age),b=spray(std::max(0.f,s.age-.009f-variation*.008f));
   if(Dot(a-s.p,s.normal)<0||Dot(b-s.p,s.normal)<0)continue;
   Streak line{};line.alpha=alpha*(.45f+variation*.55f);line.width=.55f+variation*.55f;
   if(project(a,line.x,line.y)&&project(b,line.tx,line.ty))output.splashes.push_back(line);
  }
 }
 for(unsigned i=0;i<count;++i){const auto& p=particles[i];if(!p.alive)continue;
  float candidate_x=0,candidate_y=0;
  if(!project(p.p,candidate_x,candidate_y))continue;
  // Reject snow outside the actual viewport before the five occlusion rays.
  // Include the full rotated quad plus a margin at each edge. Movement sweeps
  // still run for offscreen flakes, and visible flakes keep fresh edge tests.
  if(snow){
   const float depth=Dot(p.p-eye,forward);
   if(depth<60.f)continue;
   const float margin=3.f*p.size/(depth*tangent)+.025f;
   if(std::abs(candidate_x)>viewport_aspect.load(std::memory_order_relaxed)+margin||
      std::abs(candidate_y)>1.f+margin)continue;
  }
  // Recheck every visible candidate at the simulation rate. Cached visibility
  // across a whole particle-pool cycle leaked flakes through narrow doorways.
  if(snow?!p.visible:Blocked(ctx,base,player,eye,p.p))continue;
  if(snow){
   const V a=p.p-eye;const float depth=Dot(a,forward);
   float x=0,y=0;if(depth<60.f||!project(p.p,x,y))continue;
   const float nearFade=level==6?std::clamp((600.f-std::sqrt(Dot(a,a)))/200.f,0.f,1.f):1.f;
   const float alpha=nearFade*std::clamp(1.f-depth/2300.f,.15f,.85f)*std::clamp((depth-60.f)/100.f,0.f,1.f);
   output.snow.push_back({x,y,p.size/(depth*tangent),float(i),alpha});continue;
  }
  // Exposure-length streak follows each drop's actual velocity and size.
  const V a=p.p-eye,b=a-p.velocity*(.006f+p.size*.0015f);
  const float depth=Dot(a,forward),end_depth=Dot(b,forward);
  if(depth<100||end_depth<100)continue;
  const float x=Dot(a,right)/(depth*tangent),y=-Dot(a,up)/(depth*tangent);
  const float tx=Dot(b,right)/(end_depth*tangent),ty=-Dot(b,up)/(end_depth*tangent);
  if(std::abs(x)>3.f||std::abs(y)>1.2f)continue;
  if(Blocked(ctx,base,player,eye,eye+b))continue;
  const float nearFade=std::clamp((depth-100.f)/140.f,0.f,1.f);
  const float farFade=std::clamp(1.f-depth/2200.f,0.f,1.f);
  if(!nativeRain)output.lines.push_back({x,y,tx,ty,nearFade*farFade*(.30f+p.size*.07f),
    std::clamp(p.size*95.f/depth,.3f,.85f)});
 }
 if(rex::cvar::Query<bool>("dev_debug_runtime")&&now>=report_next){report_next=now+std::chrono::seconds(3);std::ofstream out("mission-weather-diagnostic.txt");
  out<<"mission="<<level<<" outdoors="<<outdoors<<" streaks="<<output.lines.size()<<" snowflakes="<<output.snow.size()<<" splash_drops="<<output.splashes.size()<<" fov="<<fov<<" upward="<<output.upward<<'\n';
  out<<"rain_audio_ground_water_metal_crate_vehicle_gutter=";for(float v:acoustic_levels)out<<v<<' ';
  out<<" effects_volume="<<effects_volume<<" surface="<<last_surface_id<<" material="<<last_material_id<<" cue="<<last_surface_name<<" prop="<<last_prop_name<<'\n';}
 Publish(std::move(output));
}
std::unique_ptr<rex::ui::ImmediateTexture> LoadSnowTexture(rex::ui::ImmediateDrawer* drawer){
 if(!REXCVAR_GET(pdz_weather_enabled))return {};
 if(!drawer)return {};
 std::ifstream file(std::string(SDL_GetBasePath())+"mod_assets/weather/snowflakes.png",std::ios::binary);
 std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
 if(bytes.empty())return {};
 int w=0,h=0;auto rgba=rex::ui::DecodeImageRGBA(bytes.data(),bytes.size(),w,h);
 if(rgba.empty()||w<=0||h<=0)return {};
 return drawer->CreateTexture(w,h,rex::ui::ImmediateTextureFilter::kLinear,false,rgba.data());
}
void Draw(rex::ui::ImmediateTexture* snow_texture){
 if(!REXCVAR_GET(pdz_weather_enabled))return;
 // Native pause UI is already in the game framebuffer. Never composite host
 // weather over it, even if pausing stopped Tick before it cleared the frame.
 Frame current;{std::lock_guard lock(frame_mutex);current=frame;}
 const bool cinematicRain=current.cinematic&&current.mission==1&&pdz_cutscene::Active();
 if(!pdz_input::WeatherVisible(cinematicRain))return;
 if(current.cinematic&&!cinematicRain)return;
 if((current.mission==1||current.mission==2)&&!REXCVAR_GET(pdz_custom_rain))return;
 struct Drop {float x{},y{},radius{},life{},maxlife{},seed{},speed{},trail{};};
 static std::array<Drop,160> drops{};static unsigned previous=0;static float spawn=0;
 static float eye_wait=.5f,eye_time=0,eye_duration=1,eye_strength=0;
 // Check the current camera at render time too: the weather simulation runs
 // at 30 Hz, and a stale frame must not retain beads across the camera cut.
 auto* runtime=rex::Runtime::instance();
 const bool scope=cinematicRain&&AudioScopeShot(runtime?runtime->virtual_membase():nullptr);
 static bool previousScope=false,previousCinematic=false;
 if(scope!=previousScope||cinematicRain!=previousCinematic){
  drops={};spawn=scope?.15f:0.f;eye_wait=.5f;eye_time=0;eye_strength=0;
 }
 previousScope=scope;previousCinematic=cinematicRain;
 if(!current.active)return;
 if(previous!=current.mission){drops={};spawn=0;eye_wait=.5f;eye_time=0;eye_strength=0;previous=current.mission;}
 // Gravity projected onto the lens: full runoff facing forward, progressively
 // less as the lens faces the sky. A small vertical dead zone avoids jitter.
 const float upward=std::clamp(current.upward,0.f,1.f);
 const float runoff=std::clamp((std::sqrt(std::max(0.f,1.f-upward*upward))-.05f)/.95f,0.f,1.f);
 const auto* vp=ImGui::GetMainViewport();auto* draw=ImGui::GetBackgroundDrawList();
 if(vp->Size.y>0)viewport_aspect.store(vp->Size.x/vp->Size.y,std::memory_order_relaxed);
 const float scale=vp->Size.y/1080.f,dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f);
 auto screen=[&](float x,float y){return ImVec2(vp->Pos.x+vp->Size.x*.5f+x*vp->Size.y*.5f,vp->Pos.y+(y+1)*vp->Size.y*.5f);};
 // Keep cinematic weather within the picture, clear of letterbox/subtitle bars.
 const float cinematicInset=cinematicRain?vp->Size.y*.12f:0.f;
 draw->PushClipRect({vp->Pos.x,vp->Pos.y+cinematicInset},
   {vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y-cinematicInset});
 auto streak=[&](const Streak& p,bool splash){
  const auto head=screen(p.x,p.y);auto tail=screen(p.tx,p.ty);
  if(!splash){
   const float span=std::hypot(head.x-tail.x,head.y-tail.y);
   const float maximum=22.f*scale;
   if(span>maximum){tail.x=head.x+(tail.x-head.x)*maximum/span;
    tail.y=head.y+(tail.y-head.y)*maximum/span;}
  }
  const float dx=head.x-tail.x,dy=head.y-tail.y,length=std::sqrt(dx*dx+dy*dy);
  if(length<.15f){draw->AddCircleFilled(head,std::max(.45f,p.width*scale*.5f),IM_COL32(207,219,225,int(p.alpha*125)),6);return;}
  const float width=std::max(.45f,p.width*scale),nx=-dy/length,ny=dx/length;
  // A tapered, softly fading exposure avoids the solid blue wire appearance.
  for(int i=0;i<4;++i){
   const float t0=float(i)/4.f,t1=float(i+1)/4.f;
   const float w0=width*(.12f+.38f*t0),w1=width*(.12f+.38f*t1);
   const ImVec2 a{tail.x+dx*t0,tail.y+dy*t0},b{tail.x+dx*t1,tail.y+dy*t1};
   draw->AddQuadFilled({a.x+nx*w0,a.y+ny*w0},{b.x+nx*w1,b.y+ny*w1},
     {b.x-nx*w1,b.y-ny*w1},{a.x-nx*w0,a.y-ny*w0},
     IM_COL32(218,222,225,int(p.alpha*(splash?170.f:130.f)*(.15f+.85f*t1))));
  }
 };
 for(const auto& p:current.lines)streak(p,false);
 for(const auto& p:current.splashes)streak(p,true);
 if(snow_texture)for(const auto& p:current.snow){
  const auto pos=screen(p.x,p.y);
  const float radius=std::clamp(p.tx*vp->Size.y,.9f*scale,11.f*scale);
  const unsigned variant=unsigned(p.ty)%4;
  const float u=float(variant%2)*.5f,v=float(variant/2)*.5f;
  const float angle=float(ImGui::GetTime())*(.15f+float(variant)*.08f)+p.ty*1.37f;
  const float c=std::cos(angle)*radius,s=std::sin(angle)*radius;
  draw->AddImageQuad(reinterpret_cast<ImTextureID>(snow_texture),
   {pos.x-c+s,pos.y-s-c},{pos.x+c+s,pos.y+s-c},
   {pos.x+c-s,pos.y+s+c},{pos.x-c-s,pos.y-s+c},
   {u,v},{u+.5f,v},{u+.5f,v+.5f},{u,v+.5f},IM_COL32(255,255,255,int(p.alpha*255)));
 }
 if(current.mission==7||(cinematicRain&&!scope)){draw->PopClipRect();return;}
 const bool rain_lens=current.mission==1||current.mission==2;
 // Irregular water-in-the-eyes pulses while exposed and looking into rain.
 // Blue selects the scene blur shader; ordinary beads and trails keep blue=0.
 const float eye_exposure=!cinematicRain&&current.outdoors&&rain_lens?std::clamp((upward-.5f)/.4f,0.f,1.f):0.f;
 eye_wait-=dt*eye_exposure;
 if(eye_exposure>0&&eye_wait<=0&&eye_time<=0){
  static std::mt19937 eye_rng{std::random_device{}()};
  auto random=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(eye_rng);};
  eye_duration=random(.65f,1.3f);eye_time=eye_duration;
  eye_wait=random(1.2f,3.6f);eye_strength=random(.55f,.9f);
 }
 if(eye_time>0){
  eye_time=std::max(0.f,eye_time-dt*(eye_exposure>0?1.f:4.f));
  const float phase=1.f-eye_time/eye_duration;
  const float envelope=std::sin(phase*3.141593f);
  const float strength=std::max(0.f,envelope)*eye_strength;
  if(strength>.005f)draw->AddImage(ImTextureID(1),vp->Pos,
   {vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y},{0,0},{1,1},
   IM_COL32(int(255*strength),0,255,int(230*strength)));
 }
 // Small translucent beads slide down and fade after entering cover. The
 // central aim area is kept clear; neither layer captures gameplay input.
 spawn+=current.outdoors?dt:0;
 const unsigned limit=current.mission==1?64:current.mission==4?96:160;
 // Looking into the sky restores the previous accumulation rate. Keep the
 // existing capacity so accumulated beads remain when the player looks down.
 const float pileup=std::clamp((upward-.75f)/.23f,0.f,1.f);
 const float snow_interval=.28125f-(.28125f-.09f)*pileup;
 const float river_interval=(.055f/.3f)-((.055f/.3f)-.055f)*pileup;
 const float interval=current.mission==1?.15f:current.mission==4?snow_interval:
   current.mission==6?river_interval:.055f;
 if(spawn>=interval){spawn=0;for(unsigned i=0;i<limit;++i)if(drops[i].life<=0){
   // Render-only RNG avoids sharing the simulation generator across threads.
   static std::mt19937 rng{0x44524f50};auto r=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
   float x=r(.03f,.97f),y=r(.02f,.78f);if(std::abs(x-.5f)<.13f&&std::abs(y-.5f)<.15f)x=x<.5f?.3f:.7f;
   float life=r(9.f,18.f),radius=rain_lens?r(2.5f,11.f):r(3.f,16.f);
   drops[i]={x,y,radius,life,life,r(0.f,6.283185f),radius>6.f?(rain_lens?r(.012f,.035f):r(.006f,.021f)):0.f,0.f};break;}}
 for(auto& p:drops){if(p.life<=0)continue;
  // Slower drainage retains beads longer; near vertical, incoming rain builds
  // up on the lens. Keep a small aging rate so fresh beads still fade in.
  const float aging=dt*(current.outdoors?std::max(.1f,runoff):1.f);
  p.life=runoff==0.f&&current.outdoors?std::max(1.f,p.life-aging):p.life-aging;
  const float movement=dt*p.speed*runoff;
  p.y+=movement;p.trail=std::min(p.trail+movement,.24f);
  if(p.y>1.04f){p.life=0;continue;}
  float fade=std::clamp(std::min(p.life,p.maxlife-p.life)*2.f,0.f,1.f);
  ImVec2 pos{vp->Pos.x+p.x*vp->Size.x,vp->Pos.y+p.y*vp->Size.y};float radius=p.radius*scale;
  // Each quad is shaded per pixel from a curved water height field.
  // Red carries shape seed, green distinguishes a rivulet from a bead.
  int seed=int(p.seed/6.283185f*255.f);
  const int style=rain_lens?64:0;
  const float trail=p.trail*vp->Size.y;
  if(trail>radius*2.f){
   draw->AddImage(ImTextureID(1),{pos.x-radius,pos.y-trail},
     {pos.x+radius,pos.y+radius*.3f},{0,0},{1,1},
     IM_COL32(seed,255,style,int((rain_lens?150:210)*fade)));
  }
  float stretch=rain_lens?(1.f+(p.speed>0?.18f:0.f)):(1.15f+(p.speed>0?.45f:0.f));
  draw->AddImage(ImTextureID(1),{pos.x-radius,pos.y-radius*stretch},
    {pos.x+radius,pos.y+radius*stretch},{0,0},{1,1},
    IM_COL32(seed,0,style,int(255*fade)));

 }
 draw->PopClipRect();
}
}






// Runs only on a live native emitter, avoiding cached pointers to recycled slots.
REX_HOOK_RAW(sub_82613498){
 const auto emitter=ctx.r3.u32;
 using namespace pdz_weather;
 if(breath_tracking&&breath_definition&&Valid(emitter,180)&&
    REX_LOAD_U32(emitter+156)==breath_definition){
  Write(base,emitter+64,breath_mouth);Write(base,emitter+32,breath_direction);
  auto call=ctx;call.r3.u64=emitter;sub_82614480(call,base);
 }
 __imp__sub_82613498(ctx,base);
}

