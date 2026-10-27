#include "generated/default/perfectdarkzerorecomp_init.h"
#include "nightclub_population.h"
#include "perfect_dark_zero_mode.h"
#include "npc_placement_policy.h"
#include "mousehook.h"
#include "nightclub_suspicion.h"
#include "utils.h"
#include <SDL3/SDL.h>
#include <map>
#include <rex/runtime.h>
#include <rex/hook.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <bit>
#include <vector>
#include <list>
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>

REX_EXTERN(__imp__sub_825104D8);
namespace pdz_population {
namespace {
struct Actor {uint32_t address{},setup{};unsigned copies{},candidate{};bool clone{};
  std::chrono::steady_clock::time_point retry{};};
std::list<Actor> actors;
std::vector<uint32_t> allocations;
bool spawning=false;
struct Diagnostics {unsigned ticks{},eligible{},health_rejected{},spacing{},ground{},path{},footprint{},spawn_attempts{},spawned{};float last_health{},ground_delta{};} diagnostic;
void Report() {
  static auto next=std::chrono::steady_clock::time_point{};
  const auto now=std::chrono::steady_clock::now();if(now<next)return;
  next=now+std::chrono::seconds(5);
  std::ofstream out("nightclub-population-diagnostic.txt",std::ios::trunc);
  out<<"registered="<<actors.size()<<" ticks="<<diagnostic.ticks<<" eligible="<<diagnostic.eligible
    <<" health_rejected="<<diagnostic.health_rejected<<" last_health="<<diagnostic.last_health
    <<" spacing="<<diagnostic.spacing<<" ground="<<diagnostic.ground<<" ground_delta="<<diagnostic.ground_delta
    <<" path="<<diagnostic.path<<" footprint="<<diagnostic.footprint
    <<" spawn_attempts="<<diagnostic.spawn_attempts<<" spawned="<<diagnostic.spawned<<'\n';
}
unsigned balcony_placed=0,balcony_cursor=0;
std::array<bool,4> balcony_done{};
std::mt19937 placement_random{std::random_device{}()};
// Four landing reinforcements redistributed to the user's annotated positions:
// stair top, far balcony, middle balcony, and near balcony.
constexpr std::array<Point,4> balcony_positions{{
  {2740.f,1030.f,-2220.f},{1450.f,1030.f,-4470.f},
  {2170.f,1030.f,-4470.f},{2680.f,1030.f,-4020.f}}};
// Club footprint from the live guest/guard positions; classify by authored
// spawn position so patrol movement cannot change an actor's quota.
bool InsideClub(Point p) {
  return p.x>=-3800.f&&p.x<=3000.f&&p.z>=-4900.f&&p.z<=-650.f;
}
unsigned ExtraCopies(uint8_t* base,const Actor& a) {
  const Point p{std::bit_cast<float>(REX_LOAD_U32(a.setup+4)),
    std::bit_cast<float>(REX_LOAD_U32(a.setup+8)),
    std::bit_cast<float>(REX_LOAD_U32(a.setup+12))};
  if(!InsideClub(p))return 1; // Outside: twice the original population.
  const auto body=REX_LOAD_U32(a.address+5280);
  return body>=79&&body<=82?863:2; // Another 6x: 144 per original becomes 864 total.
}
auto next_tick=std::chrono::steady_clock::time_point{};
bool Readable(uint32_t p,uint32_t length) {
  auto* rt=rex::Runtime::instance();if(!rt||!p||uint64_t(p)+length>0x100000000ull)return false;
  for(uint64_t at=p;at<uint64_t(p)+length;) {
    auto* h=rt->memory()->LookupHeap(uint32_t(at));uint32_t protection=0;
    if(!h||!h->QueryProtect(uint32_t(at),&protection)||!(protection&rex::memory::kMemoryProtectRead))return false;
    at=(at/h->page_size()+1)*h->page_size();
  }
  return true;
}
Point Read(uint8_t* base,uint32_t p) {
  return {std::bit_cast<float>(REX_LOAD_U32(p)),std::bit_cast<float>(REX_LOAD_U32(p+4)),std::bit_cast<float>(REX_LOAD_U32(p+8))};
}
void Write(uint8_t* base,uint32_t p,Point v) {
  REX_STORE_U32(p,std::bit_cast<uint32_t>(v.x));REX_STORE_U32(p+4,std::bit_cast<uint32_t>(v.y));REX_STORE_U32(p+8,std::bit_cast<uint32_t>(v.z));
}
bool Ray(PPCContext call,uint8_t* base,uint32_t ignore,Point from,Point to,Point* contact=nullptr) {
  call.r1.u32-=4096;if(!Readable(call.r1.u32,4096))return true;
  const auto a=call.r1.u32+256,b=a+16,out=b+16,point=out+16;
  Write(base,a,from);Write(base,b,to);REX_STORE_U32(out,0);
  REX_STORE_U32(call.r1.u32+84,0);REX_STORE_U32(call.r1.u32+92,0);REX_STORE_U32(call.r1.u32+100,0);
  call.r3.u64=ignore;call.r4.u64=a;call.r5.u64=b;call.r6.u64=4;
  call.r7.u64=out;call.r8.u64=0;call.r9.u64=point;call.r10.u64=0;
  sub_82239358(call,base);
  if(contact)*contact=Read(base,point);
  return call.r3.u8!=0;
}
bool Placement(PPCContext call,uint8_t* base,uint32_t source,Point origin,Point& p,
               std::span<const Point> occupied,bool local_path=true) {
  if(!Separated(p,occupied)){++diagnostic.spacing;return false;}
  // Actor transforms are not necessarily at the soles. Derive the native
  // origin-to-floor offset from the source rather than treating it as zero.
  Point source_floor{},ground{};
  if(!Ray(call,base,source,{origin.x,origin.y+30,origin.z},
          {origin.x,origin.y-350,origin.z},&source_floor)||!Finite(source_floor)) {
    ++diagnostic.ground;return false;
  }
  const float sole_offset=origin.y-source_floor.y;
  if(sole_offset < -30.f || sole_offset > 350.f) {
    ++diagnostic.ground;diagnostic.ground_delta=sole_offset;return false;
  }
  if(!Ray(call,base,source,{p.x,source_floor.y+60,p.z},
          {p.x,source_floor.y-60,p.z},&ground)||!Finite(ground)||
          std::abs(ground.y-source_floor.y)>55.f) {
    ++diagnostic.ground;diagnostic.ground_delta=ground.y-source_floor.y;return false;
  }
  p.y=ground.y+sole_offset+2.f;
  if(!Separated(p,occupied)){++diagnostic.spacing;return false;}
  // Check the path from the original actor and the new capsule's footprint.
  // A clear centre ray alone would allow half a character to intersect a wall.
  for(float height:{35.f,100.f,165.f}) {
    if(local_path&&(std::abs(origin.x-p.x)+std::abs(origin.z-p.z)>1.f)&&
       Ray(call,base,source,{origin.x,source_floor.y+height,origin.z},{p.x,ground.y+height,p.z})){++diagnostic.path;return false;}
    for(Point d:{Point{45,0,0},Point{-45,0,0},Point{0,0,45},Point{0,0,-45}})
      if(Ray(call,base,source,{p.x,ground.y+height,p.z},{p.x+d.x,ground.y+height,p.z+d.z})){++diagnostic.path;return false;}
  }
  for(Point d:{Point{35,0,35},Point{-35,0,35},Point{35,0,-35},Point{-35,0,-35}}) {
    Point edge{};
    if(!Ray(call,base,source,{p.x+d.x,ground.y+35,p.z+d.z},{p.x+d.x,ground.y-40,p.z+d.z},&edge)||
       !Finite(edge)||std::abs(edge.y-ground.y)>25){++diagnostic.footprint;return false;}
  }
  return true;
}
bool Eligible(PPCContext call,uint8_t* base,const Actor& a,uint32_t player) {
  const auto table=REX_LOAD_U32(a.address);
  if(!Readable(table,336))return false;
  const auto fn=REX_LOAD_U32(table+332);if(fn<0x82100000||fn>=0x82D00000)return false;
  auto health=call;health.r1.u32-=2048;health.r3.u64=a.address;health.r4.u64=0;
  {auto ctx=health;REX_CALL_INDIRECT_FUNC(fn);health=ctx;}
  diagnostic.last_health=float(health.f1.f64);
  if(!std::isfinite(health.f1.f64)||health.f1.f64<=0){++diagnostic.health_rejected;return false;}
  const auto body=REX_LOAD_U32(a.address+5280);
  if(body==7||body==113||body==30||body==99||body==11||body==12)return false;
  if(body>=79&&body<=82)return true; // Native Nightclub guest/dancer bodies.
  if(!REX_LOAD_U32(a.setup+184)&&!REX_LOAD_U32(a.setup+196))return false;
  call.r1.u32-=2048;call.r3.u64=a.address;call.r4.u64=player;
  __imp__sub_825104D8(call,base);
  return !call.r3.u8; // Excludes friendly story actors, including Jack.
}
// Use the complete native nightclub behavior installed by sub_822AA9E0.
// A bare animation task can be displaced by the clone's default AI; this
// behavior owns the dance and the native danger/evacuation transitions.
void StartDance(PPCContext call,uint8_t* base,uint32_t actor,uint32_t body) {
  if(body<79||body>82)return;
  const uint32_t variant=(body==80||body==82)?4+placement_random()%2:1+placement_random()%3;
  call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
  call.r3.u64=120;sub_821739E8(call,base);
  if(!call.r3.u32)return;
  call.r4.u64=0;call.r5.u64=variant;sub_821C3ED0(call,base);
  const auto behavior=call.r3.u32;
  // Match the authored mission's transition out of the default behavior.
  const auto table=REX_LOAD_U32(actor);
  call.r3.u64=actor;call.r4.u64=0;
  const auto reset=REX_LOAD_U32(table+828);
  {auto ctx=call;REX_CALL_INDIRECT_FUNC(reset);call=ctx;}
  call.r3.u64=actor+6356;call.r4.u64=behavior;sub_8216C780(call,base);
}
bool Spawn(PPCContext call,uint8_t* base,Actor& original,Point point) {
  if(actors.size()>=1024)return false; // Never create an untracked reinforcement.
  ++diagnostic.spawn_attempts;
  const auto source_actor=original.address;
  const auto source_body=REX_LOAD_U32(source_actor+5280);
  const bool guest=source_body>=79&&source_body<=82;
  // Mission scripts assign civilian allegiance to the authored actor after
  // construction. Its setup block alone does not contain that runtime state.
  const std::array<uint16_t,3> allegiance{REX_LOAD_U16(source_actor+128),
    REX_LOAD_U16(source_actor+130),REX_LOAD_U16(source_actor+132)};
  const auto factory=REX_LOAD_U32(0x82D6B2B0+76);
  if(!Readable(factory,8))return false;
  call.r1.u32-=4096;call.r3.u64=factory;call.r4.u64=33;sub_82513DD8(call,base);
  const auto size=call.r3.u32;
  if(size<268||size>4096||!Readable(original.setup,size))return false;
  auto* memory=rex::Runtime::instance()->memory();
  const auto setup=memory->SystemHeapAlloc(size+16,16);if(!setup)return false;
  allocations.push_back(setup);
  std::memcpy(base+setup,base+original.setup,size);
  base[setup+size]=0;REX_STORE_U32(setup+52,setup+size);
  Write(base,setup+4,point);
  spawning=true;
  call.r3.u64=factory;call.r4.u64=33;call.r5.u64=setup;call.r6.u64=1;call.r7.u64=1;
  sub_82513D00(call,base);
  spawning=false;
  if(!Readable(call.r3.u32,13040))return false;
  const auto spawned_actor=call.r3.u32;
  if(guest){
    // Copy only the native allegiance masks, never AI pointers or task lists.
    for(unsigned i=0;i<allegiance.size();++i)
      REX_STORE_U16(spawned_actor+128+i*2,allegiance[i]);
    REXLOG_INFO("Nightclub civilian clone: source {:08X} clone {:08X} allegiance {}/{}/{}",
      source_actor,spawned_actor,allegiance[0],allegiance[1],allegiance[2]);
  }
  StartDance(call,base,spawned_actor,REX_LOAD_U32(spawned_actor+5280));
  ++diagnostic.spawned;
  // Native construction can invoke callbacks; reacquire the source entry.
  for(auto& a:actors)if(a.address==source_actor) {
    ++a.copies;
    REXLOG_INFO("Nightclub population: extra {} actor {:08X} source {:08X} at ({:.1f},{:.1f},{:.1f})",
      a.copies,call.r3.u32,source_actor,point.x,point.y,point.z);
    break;
  }
  return true;
}
#include "nightclub_suspicion.inl"
}
bool FriendlyOverride(uint8_t* base,uint32_t a,uint32_t b,bool native_friend,bool& result) {
  if(!SocialMission(base))return false;
  const auto player=SocialPlayer(base);
  const auto guard=a==player?b:(b==player?a:0);
  if(!guard||guard==player)return false;
  auto found=suspicion.find(guard);
  if(found==suspicion.end()) {
    if(!SocialGuard(base,guard)||(native_friend&&REX_LOAD_U32(guard+5280)!=48))return false;
    found=suspicion.try_emplace(guard).first;
  }
  result=found->second.exposure.Stage()!=pdz_nightclub::SuspicionStage::Hostile&&Unarmed(base,player);
  return true;
}
bool HoldFire(uint8_t* base,uint32_t shooter) {
  if(!SocialMission(base))return false;
  const auto found=suspicion.find(shooter);
  if(found==suspicion.end())return false;
  if(!Unarmed(base,SocialPlayer(base))){found->second.exposure.hostile=true;return false;}
  return found->second.exposure.Stage()!=pdz_nightclub::SuspicionStage::Hostile;
}
void Created(uint8_t* base,uint32_t actor,uint32_t setup) {
  if(!Readable(actor,13040)||!Readable(setup,268))return;
  for(const auto& a:actors)if(a.address==actor)return;
  if(actors.size()<1024)actors.push_back({actor,setup,0,0,spawning});
}
void Destroyed(uint32_t actor) {pdz_custom::Forget(actor);suspicion.erase(actor);actors.remove_if([&](const Actor& a){return a.address==actor;});}
void LevelEnded() {
  pdz_custom::Reset();
  actors.clear();
  diagnostic={};
  suspicion.clear();social_last={};reply_pending=false;StopReply();
  for(auto p:allocations)rex::Runtime::instance()->memory()->SystemHeapFree(p);
  allocations.clear();next_tick={};spawning=false;balcony_placed=balcony_cursor=0;balcony_done.fill(false);
}
void Tick(PPCContext& ctx,uint8_t* base) {
  SocialTick(ctx,base);
  ++diagnostic.ticks;Report();
  if(!pdz_input::Supported()||spawning)return;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(!Readable(session,40)||REX_LOAD_U8(session+25)!=1||REX_LOAD_U32(session+36)>1||REX_LOAD_U8(session))return;
  const auto owner=REX_LOAD_U32(0x82E35468);
  if(!Readable(owner,956))return;
  const auto player=REX_LOAD_U32(owner+952);if(!Readable(player,60))return;
  const auto now=std::chrono::steady_clock::now();if(now<next_tick)return;
  next_tick=now+std::chrono::milliseconds(50);
  std::vector<Point> occupied{Read(base,player+48)};
  for(const auto& a:actors)if(Readable(a.address,60))occupied.push_back(Read(base,a.address+48));
  for(auto& a:actors) {
    if(a.clone||now<a.retry||!Readable(a.address,13040)||!Readable(a.setup,268))continue;
    if(a.copies>=ExtraCopies(base,a))continue;
    if(a.candidate>=32)a.candidate=0;
    if(!Eligible(ctx,base,a,player))continue;
    ++diagnostic.eligible;
    const auto origin=Read(base,a.address+48);if(!Finite(origin))continue;
    // Move the four extra landing guards, rather than adding another group.
    if(REX_LOAD_U32(a.address+5280)==48&&origin.x>2450.f&&origin.x<3000.f&&
       origin.y>450.f&&origin.y<700.f&&origin.z>-3350.f&&origin.z<-2400.f&&
       balcony_placed<balcony_positions.size()) {
      unsigned slot=(balcony_cursor++)%balcony_positions.size();
      for(unsigned i=0;i<4&&balcony_done[slot];++i)slot=(slot+1)%4;
      const auto anchor=balcony_positions[slot];
      auto point=anchor;
      // Search around each marked spot independently; an obstructed stair-top
      // sample must not prevent the three balcony positions from being tried.
      if(a.candidate++%5) {
        std::uniform_real_distribution<float> jitter(-180.f,180.f);
        point.x+=jitter(placement_random);point.z+=jitter(placement_random);
      }
      // Test support at the destination itself; the straight line from the
      // lower landing crosses railings and cannot validate an upstairs spot.
      if(Placement(ctx,base,a.address,point,point,occupied,false)&&Spawn(ctx,base,a,point)) {
        balcony_done[slot]=true;++balcony_placed;return;
      }
      a.retry=now+std::chrono::seconds(2);
      return;
    }
    // Bound expensive geometry work per frame; try other slots on later ticks.
    for(unsigned attempt=0;attempt<2&&a.candidate<32;++attempt) {
      const auto body=REX_LOAD_U32(a.address+5280);
      const bool dancer=body>=79&&body<=82&&InsideClub(origin);
      auto point=Candidate(origin,a.candidate++);
      if(dancer) {
        // Independent positions across the occupied club floor, not a ring of
        // identical dancers around every original. Native geometry still checks
        // support, capsule clearance and the whole footprint.
        // Sample the full club footprint on both levels instead of small
        // islands around the guard markers. Geometry rejects the central void,
        // bars, tables, stairs and unsupported balcony edges.
        std::uniform_real_distribution<float> x(-2700.f,2850.f),z(-4650.f,-850.f);
        point={x(placement_random),a.copies%2?1030.f:origin.y,z(placement_random)};
        if(!Separated(point,occupied,180.f))continue;
      }
      if(Placement(ctx,base,a.address,dancer?point:origin,point,occupied,!dancer)&&(!dancer||Separated(point,occupied,180.f))&&Spawn(ctx,base,a,point))return;
    }
    if(a.candidate>=32)a.retry=now+std::chrono::seconds(5);
    return;
  }
}
}

// Shared native capacity extension; extra population remains Nightclub-only. Preserve native sentinel addresses
// and list headers because retail iteration references them directly.
namespace {
constexpr uint32_t kCrowdCapacity=1024,kRetailCapacity=150,kCharacterStride=13056;
uint32_t crowd_storage=0;
bool initializing_crowd_storage=false;
std::array<uint32_t,2> crowd_list_nodes{};
bool CrowdLevel(uint8_t* base) {
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  return pdz_population::Readable(session,40)&&REX_LOAD_U8(session+25)==1&&REX_LOAD_U32(session+36)<=1;
}
void ExtendCrowdLists(uint8_t* base) {
  if(!crowd_storage)return;
  constexpr std::array<uint32_t,2> lists{0x82E446D8,0x82E43FB8};
  for(unsigned list=0;list<lists.size();++list) {
    const auto header=lists[list],sentinel=header+1800;
    // The reset hook runs only after native list clearing. Never splice into
    // an active list or move its native header/sentinel.
    if(REX_LOAD_U32(header+1816)!=0||REX_LOAD_U32(header+1812)!=header)continue;
    if(!crowd_list_nodes[list])crowd_list_nodes[list]=rex::Runtime::instance()->memory()->SystemHeapAlloc((kCrowdCapacity-kRetailCapacity)*12,16);
    const auto first=crowd_list_nodes[list];if(!first)continue;
    const auto count=kCrowdCapacity-kRetailCapacity,last=first+(count-1)*12;
    for(uint32_t i=0;i<count;++i) {
      const auto node=first+i*12;
      REX_STORE_U32(node,0);
      REX_STORE_U32(node+4,i?node-12:header+1788);
      REX_STORE_U32(node+8,i+1<count?node+12:sentinel);
    }
    REX_STORE_U32(header+1796,first);
    REX_STORE_U32(sentinel+4,last);
  }
}
}
REX_EXTERN(__imp__sub_82395398);
REX_HOOK_RAW(sub_82395398) {
  const bool crowd=initializing_crowd_storage&&
    ctx.r3.u32==128&&ctx.r4.u32==kRetailCapacity*kCharacterStride&&ctx.r5.u32==3;
  const PPCContext original=ctx;
  if(crowd)ctx.r4.u64=kCrowdCapacity*kCharacterStride;
  __imp__sub_82395398(ctx,base);
  if(crowd) {
    crowd_storage=ctx.r3.u32;
    if(!crowd_storage) {
      // Retail pool construction dereferences this allocation unconditionally.
      // A larger request can exceed the native arena; retry its original size
      // and leave both the pool and registry lists at retail capacity.
      ctx=original;
      __imp__sub_82395398(ctx,base);
      REXLOG_WARN("Expanded character arena unavailable; retaining retail capacity");
    }
  }
}
REX_EXTERN(__imp__sub_82629070);
REX_HOOK_RAW(sub_82629070) {
  if(crowd_storage&&ctx.r6.u32==crowd_storage&&
     ctx.r4.u32==kCharacterStride&&ctx.r5.u32==kRetailCapacity)ctx.r5.u64=kCrowdCapacity;
  __imp__sub_82629070(ctx,base);
}
REX_EXTERN(__imp__sub_821523B8);
REX_HOOK_RAW(sub_821523B8) {
  __imp__sub_821523B8(ctx,base);
  ExtendCrowdLists(base);
}

REX_EXTERN(__imp__sub_82141E48);
REX_HOOK_RAW(sub_82141E48) {
  // Recompiled direct calls do not reliably retain a native LR call-site.
  // Scope the allocation override to the actual pool initializer instead.
  initializing_crowd_storage=true;
  crowd_storage=0;
  __imp__sub_82141E48(ctx,base);
  initializing_crowd_storage=false;
  std::ofstream report("nightclub-capacity-init.txt",std::ios::trunc);
  const auto manager=REX_LOAD_U32(0x82D6B2B0+28);
  report<<"allocated_storage="<<crowd_storage;
  if(pdz_population::Readable(manager,756))report<<" capacity="<<REX_LOAD_U32(manager+744);
  report<<" nodes="<<crowd_list_nodes[0]<<","<<crowd_list_nodes[1]<<'\n';
}



