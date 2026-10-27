#include "generated/default/perfectdarkzerorecomp_init.h"
#include "mission_weather.h"
#include "mousehook.h"
#include "weather_audio.h"
#include <rex/runtime.h>
#include <rex/cvar.h>
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

REXCVAR_DEFINE_BOOL(pdz_weather_enabled, true, "Gameplay", "Enable added weather, lens effects, splashes and weather audio");
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
 sub_82239358(call,base);if(contact&&call.r3.u8)*contact=Read(base,hit+16);
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
 const auto manager=REX_LOAD_U32(0x82D6B2E0);if(!Valid(manager,60))return {};
 const auto list=REX_LOAD_U32(manager+56);if(!Valid(list,12))return {};
 const auto table=REX_LOAD_U32(list),count=REX_LOAD_U32(list+8);
 if(count>256||!Valid(table,count*4))return {};
 for(uint32_t i=0;i<count;++i){const auto entry=REX_LOAD_U32(table+i*4);
  if(Valid(entry,136)&&REX_LOAD_U32(entry)==id)return Text(base,REX_LOAD_U32(entry+128));
 }return {};
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
   volume.r3.u64=13;volume.r4.u64=REX_LOAD_U8(0x82E66E80+13*32+28);sub_821F3E00(volume,base);
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
struct Frame {bool active=false,outdoors=false;float upward=0;unsigned mission=0;std::vector<Streak> lines,splashes,snow;};
std::mutex frame_mutex;Frame frame;
std::array<Particle,1080> particles{};
std::array<Splash,48> splashes{};
unsigned splash_cursor=0,splash_visibility=0;
unsigned mission=0,cursor=0;
std::mt19937 random{0x5241494e};
float Random(float lo,float hi){return std::uniform_real_distribution<float>(lo,hi)(random);}
auto last=std::chrono::steady_clock::time_point{};
auto roof_next=last,report_next=last;
bool outdoors=false;
void Publish(Frame value){std::lock_guard lock(frame_mutex);frame=std::move(value);}
}
void Tick(PPCContext& ctx,uint8_t* base){
 validated_probe_stack=0;
 if(!REXCVAR_GET(pdz_weather_enabled)){rain_audio.Pause();Publish({});last={};mission=0;return;}
 const auto session=REX_LOAD_U32(0x82D6B2B0);
 if(!pdz_input::Supported()||!Valid(session,40)){rain_audio.Pause();Publish({});last={};return;}
 const unsigned level=REX_LOAD_U8(session+25);
 if((level!=1&&level!=2&&level!=4&&level!=6)||REX_LOAD_U32(session+36)>1){rain_audio.Pause();Publish({});last={};mission=0;return;}
 if(mission!=level){mission=level;particles={};splashes={};cursor=0;splash_cursor=0;splash_visibility=0;roof_next={};outdoors=false;acoustic={};gutter={};acoustic_next={};acoustic_cursor=0;rain_audio.Pause();}
 if(!pdz_input::WeatherVisible()){rain_audio.Pause();Publish({});last={};return;}
 const auto camera=REX_LOAD_U32(0x82D68320);
 if(!Valid(camera,1092)){rain_audio.Pause();Publish({});last={};return;}
 const auto player=REX_LOAD_U32(camera+8),view=REX_LOAD_U32(camera+60);
 if(!Valid(player,60)||!Valid(view,240)){rain_audio.Pause();Publish({});last={};return;}
 const auto now=std::chrono::steady_clock::now();
 float dt=last.time_since_epoch().count()?float(std::chrono::duration<double>(now-last).count()):0.f;
 if(dt>0&&dt<1.f/30.f)return;last=now;dt=std::clamp(dt,0.f,.1f);
 const V eye=Read(base,view+224),right=Read(base,view+176),up=Read(base,view+192),forward=Read(base,view+208)*-1.f;
 const float fov=F(base,camera+1088);
 if(!std::isfinite(Dot(eye,eye))||!std::isfinite(fov)||fov<1||fov>175){rain_audio.Pause();Publish({});return;}
 const float tangent=std::tan(fov*3.14159265f/360.f);
 if(now>=roof_next){outdoors=!Blocked(ctx,base,player,eye,eye+V{0,5000,0});roof_next=now+std::chrono::milliseconds(300);}
 const bool snow=level==4||level==6;
 // The normal ray excludes Joanna to keep listener/roof probes usable. Only
 // falling rain in cover includes her native collision geometry (ignore=0).
 const uint32_t mode=Valid(camera,0x540)?REX_LOAD_U32(camera+0x53C):0;
 const bool cover=!snow&&Valid(mode,12)&&REX_LOAD_U8(mode+11)==3;
 const V joanna=Read(base,player+48);
 if(snow)rain_audio.Pause();else Sound(ctx,base,player,eye,right,outdoors,level);
 const unsigned count=level==6?1080:snow?720:level==1?80:240;
 const V velocity{55.f,-(level==1?1500.f:2100.f),20.f};
 for(auto& s:splashes)s.age+=dt;
 for(unsigned i=0;i<count;++i){auto& p=particles[i];if(!p.alive)continue;
  V next=p.p+p.velocity*dt,contact{};SurfaceHit surface{};
  // Sweep the actual travelled segment, including moving doors. A floor
  // sampled at birth cannot protect against a door closing across the path.
  const bool hit=dt>0&&Blocked(ctx,base,cover?0:player,p.p,next,&contact,&surface);
  if(hit&&snow){
   const float length=std::sqrt(Dot(surface.normal,surface.normal));
   if(std::isfinite(length)&&length>.01f&&std::abs(surface.normal.y/length)<.65f){
    V normal=surface.normal*(1.f/length);
    if(Dot(normal,p.velocity)>0)normal=normal*-1.f;
    // Wind slides along a wall; it never teleports a flake through it.
    p.velocity=p.velocity-normal*Dot(p.velocity,normal);
    next=contact+normal*(p.size+1.f);
    if(Blocked(ctx,base,player,p.p,next))p.alive=false;
   }else p.alive=false;
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
  if(p.p.y<eye.y-500.f||Dot(p.p-eye,p.p-eye)>3600000.f)p.alive=false;
 }
 // Bound geometry queries independently of the renderer's frame rate. Native
 // collision rejects indoor emitters and rain hidden behind solid geometry.
 for(unsigned n=0;n<(level==6?36u:snow?24u:12u);++n){auto& p=particles[(cursor++)%count];
  if(!p.alive){p.p=eye+V{Random(-1350,1350),Random(100,1000),Random(-1350,1350)};
   // Reserve part of the existing budget for close drops. They still travel
   // through the native collision query; roofs/walls win before body hits.
   if(cover&&n<3&&std::isfinite(Dot(joanna,joanna)))
     p.p=joanna+V{Random(-38,22),Random(220,340),Random(-30,30)};
   p.velocity=snow?V{Random(45.f,115.f),-Random(110.f,240.f),Random(-45.f,45.f)}:
     V{velocity.x+Random(-18.f,18.f),velocity.y*Random(.8f,1.2f),velocity.z+Random(-12.f,12.f)};
   p.size=Random(2.f,5.f);
   p.alive=!Blocked(ctx,base,player,p.p,p.p+V{0,5000,0});p.visible=false;
  }
 }
 Frame output;output.active=true;output.outdoors=outdoors;output.mission=level;
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
  if(Blocked(ctx,base,player,eye,p.p))continue;
  if(snow){
   const V a=p.p-eye;const float depth=Dot(a,forward);
   float x=0,y=0;if(depth<60.f||!project(p.p,x,y))continue;
   // Also keep the textured quad's edges behind geometry at a doorway edge.
   const float extent=p.size*1.5f;
   if(Blocked(ctx,base,player,eye,p.p+right*extent)||
      Blocked(ctx,base,player,eye,p.p-right*extent)||
      Blocked(ctx,base,player,eye,p.p+up*extent)||
      Blocked(ctx,base,player,eye,p.p-up*extent))continue;
   const float alpha=std::clamp(1.f-depth/2300.f,.15f,.85f)*std::clamp((depth-60.f)/100.f,0.f,1.f);
   output.snow.push_back({x,y,p.size/(depth*tangent),float(i),alpha});continue;
  }
  // Exposure-length streak follows each drop's actual velocity and size.
  const V a=p.p-eye,b=a-p.velocity*(.009f+p.size*.0022f);
  const float depth=Dot(a,forward),end_depth=Dot(b,forward);
  if(depth<100||end_depth<100)continue;
  const float x=Dot(a,right)/(depth*tangent),y=-Dot(a,up)/(depth*tangent);
  const float tx=Dot(b,right)/(end_depth*tangent),ty=-Dot(b,up)/(end_depth*tangent);
  if(std::abs(x)>3.f||std::abs(y)>1.2f)continue;
  if(Blocked(ctx,base,player,eye,eye+b))continue;
  output.lines.push_back({x,y,tx,ty,std::clamp(1.f-depth/2200.f,.08f,.6f),
    std::clamp(p.size*110.f/depth,.4f,1.25f)});
 }
 if(now>=report_next){report_next=now+std::chrono::seconds(3);std::ofstream out("mission-weather-diagnostic.txt");
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
 if(!pdz_input::WeatherVisible())return;
 Frame current;{std::lock_guard lock(frame_mutex);current=frame;}
 struct Drop {float x{},y{},radius{},life{},maxlife{},seed{},speed{},trail{};};
 static std::array<Drop,160> drops{};static unsigned previous=0;static float spawn=0;
 static float eye_wait=.5f,eye_time=0,eye_duration=1,eye_strength=0;
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
 draw->PushClipRect(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y});
 auto streak=[&](const Streak& p,bool splash){
  const auto head=screen(p.x,p.y),tail=screen(p.tx,p.ty);
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
     IM_COL32(207,219,225,int(p.alpha*(splash?170.f:130.f)*(.15f+.85f*t1))));
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
 const bool rain_lens=current.mission==1||current.mission==2;
 // Irregular water-in-the-eyes pulses while exposed and looking into rain.
 // Blue selects the scene blur shader; ordinary beads and trails keep blue=0.
 const float eye_exposure=current.outdoors&&rain_lens?std::clamp((upward-.5f)/.4f,0.f,1.f):0.f;
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

