#include "generated/default/perfectdarkzerorecomp_init.h"
#include "coop_ally.h"
#include "coop_ally_logic.h"
#include "coop_sync_logic.h"
#include "game_cvars.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include "klobb_weapon.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/system/xam/virtual_coop.h>
#include <rex/system/xam/virtual_coop_input.h>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <vector>

namespace {
struct Character { uint32_t address{}, setup{}; uint64_t generation{}; };
std::array<Character,192> characters{};
uint64_t generation=0;
uint32_t ally=0, player=0, owned_setup=0, follow_reference=0;
uint8_t combat_script=0;
bool attempted=false, spawning=false, changing_script=false;
bool bot_coop_selected=false;
bool native_partner_selected=false;
pdz_coop::Order last_order=pdz_coop::Order::Hold;
pdz_coop::AllyPolicy policy;
pdz_coop::RescuePolicy rescue;
pdz_coop::SyncPolicy sync_policy;
std::atomic<int> sync_status{0};
bool sync_formation=false, sync_firing=false;
uint32_t sync_player_weapon=0, sync_weapon=0;
unsigned sync_saved_weapon=0;
pdz_coop::Position sync_destination{};
auto sync_sample=std::chrono::steady_clock::time_point{};
std::atomic<bool> use_held{false}, player_downed{false};
std::atomic<int> rescue_status{0};
bool restoring_health=false;
float ally_full_health=100.f;
uint32_t assisted_puzzle=0;
float assist_seconds=0;
auto next_tick=std::chrono::steady_clock::time_point{};
auto next_follow=std::chrono::steady_clock::time_point{};
pdz_coop::Position follow_destination{};
bool follow_direct=false;
uint32_t follow_pad=0;
int follow_gait=-1;
bool Readable(uint32_t address,uint32_t length) {
  auto* rt=rex::Runtime::instance();
  if(!rt||!address||uint64_t(address)+length>0x100000000ull)return false;
  for(uint64_t p=address;p<uint64_t(address)+length;) {
    auto* h=rt->memory()->LookupHeap(uint32_t(p));uint32_t protection=0;
    if(!h||!h->QueryProtect(uint32_t(p),&protection)||!(protection&rex::memory::kMemoryProtectRead))return false;
    p=(p/h->page_size()+1)*h->page_size();
  }
  return true;
}
pdz_coop::Actor Handle(uint32_t address) {
  if(address==player&&player)return (uint64_t(1)<<63)|player;
  for(const auto& c:characters)if(c.address==address&&address)return (c.generation<<32)|address;
  return 0;
}
bool Campaign(uint8_t* base) {
  if(native_partner_selected)return false; // Never run NPC-only offsets against Player 2.
  if(!bot_coop_selected||!pdz_input::Supported()||!REXCVAR_GET(pdz_coop_ally))return false;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  return Readable(session,40)&&REX_LOAD_U32(session+36)==0;
}
float Health(PPCContext ctx,uint8_t* base,uint32_t actor) {
  if(!Readable(actor,4))return 0;
  const auto table=REX_LOAD_U32(actor);
  if(!Readable(table,336))return 0;
  const auto fn=REX_LOAD_U32(table+332);
  if(fn<0x82100000||fn>=0x82D00000)return 0;
  ctx.r1.u32-=2048;ctx.r3.u64=actor;ctx.r4.u64=0;
  REX_CALL_INDIRECT_FUNC(fn);
  return std::isfinite(ctx.f1.f64)?float(ctx.f1.f64):0.f;
}
pdz_coop::Position ActorPosition(uint8_t* base,uint32_t actor) {
  return {std::bit_cast<float>(REX_LOAD_U32(actor+48))*.01f,
          std::bit_cast<float>(REX_LOAD_U32(actor+52))*.01f,
          std::bit_cast<float>(REX_LOAD_U32(actor+56))*.01f};
}
uint32_t Target(uint8_t* base,uint32_t actor) {
  if(!Readable(actor,12656))return 0;
  const auto priority=REX_LOAD_U32(actor+12652);
  return priority?priority:REX_LOAD_U32(actor+12628);
}
bool HostileToPlayer(PPCContext ctx,uint8_t* base,uint32_t actor) {
  if(!player||actor==ally||actor==player||!Readable(actor,136))return false;
  ctx.r1.u32-=2048;ctx.r3.u64=actor;ctx.r4.u64=player;
  sub_825104D8(ctx,base);
  return !ctx.r3.u8;
}
bool Sees(PPCContext ctx,uint8_t* base,uint32_t observer,uint32_t target) {
  ctx.r1.u32-=2048;ctx.r3.u64=observer;ctx.r4.u64=target;ctx.r5.u64=1;
  sub_82508D88(ctx,base);return ctx.r3.u8!=0;
}
void SetScript(PPCContext ctx,uint8_t* base,uint8_t script) {
  ctx.r1.u32-=2048;ctx.r3.u64=script;sub_82173028(ctx,base);
  const auto instance=ctx.r3.u32;
  if(!Readable(instance,56))return;
  changing_script=true;
  ctx.r3.u64=ally+6356;ctx.r4.u64=instance;sub_8216C780(ctx,base);
  changing_script=false;
}
void StopFollowing(PPCContext ctx,uint8_t* base);
void FollowGait(PPCContext call,uint8_t* base,bool run) {
  const int gait=run?109:107;
  if(follow_gait==gait)return;
  const auto navigation=REX_LOAD_U32(ally+12528);
  if(!Readable(navigation,192))return;
  // Use the native locomotion animation's root speed, as patrol/combat AI do.
  call.r1.u32-=2048;call.r3.u64=ally;call.r4.u64=gait;call.r5.u64=0;
  sub_821E8FB8(call,base);
  const auto animation=call.r3.u32;
  if(!Readable(animation,24))return;
  const auto clip=REX_LOAD_U32(animation+20);
  if(!Readable(clip,12)||!REX_LOAD_U32(clip+8))return;
  sub_821EAB90(call,base);
  const float maximum=std::bit_cast<float>(REX_LOAD_U32(navigation+188));
  const float speed=float(call.f1.f64);
  if(!std::isfinite(speed)||speed<=0||!std::isfinite(maximum)||maximum<=0)return;
  REX_STORE_U32(navigation+184,std::bit_cast<uint32_t>(std::min(speed,maximum)));
  follow_gait=gait;
}
void Follow(PPCContext ctx,uint8_t* base) {
  if(!Readable(follow_reference,8)||!Readable(player,184)||!Readable(ally,13040))return;
  const auto now=std::chrono::steady_clock::now();
  if(now<next_follow)return;
  next_follow=now+std::chrono::milliseconds(600);
  const auto destination=ActorPosition(base,player);
  if(!std::isfinite(destination.x)||!std::isfinite(destination.y)||!std::isfinite(destination.z))return;
  const auto origin=ActorPosition(base,ally);
  const float height=std::abs(destination.y-origin.y);
  const float distance=(destination.x-origin.x)*(destination.x-origin.x)+
                       (destination.z-origin.z)*(destination.z-origin.z);
  FollowGait(ctx,base,distance>16.f||height>.5f);
  // Visibility alone does not make a walkable shortcut (stairs/railings).
  const bool direct=height<.45f&&Sees(ctx,base,ally,player);
  const auto pad=REX_LOAD_U32(player+180);
  // The game owns queued tasks. This intrusive weak reference is cleared on
  // destruction, so completion cannot leave a reused task allocation here.
  const auto previous=REX_LOAD_U32(follow_reference+4);
  if(previous&&Readable(previous,36)&&REX_LOAD_U32(previous+12)<2) {
    const float dx=destination.x-follow_destination.x,dy=destination.y-follow_destination.y,
                dz=destination.z-follow_destination.z;
    if(direct==follow_direct&&(direct?dx*dx+dy*dy+dz*dz<1.f:pad==follow_pad))return;
    StopFollowing(ctx,base);
    FollowGait(ctx,base,distance>16.f||height>.5f);
  }
  if(!direct&&!Readable(pad,66))return;
  if(!direct) {
    const auto navigation=REX_LOAD_U32(ally+12528);
    if(!Readable(navigation,192))return;
    auto refresh=ctx;refresh.r1.u32-=2048;refresh.r3.u64=navigation;refresh.r4.u64=1;
    sub_8218B050(refresh,base);
  }
  ctx.r1.u32-=2048;ctx.r3.u64=direct?72:96;sub_82173980(ctx,base);
  const auto task=ctx.r3.u32;
  if(!task)return;
  ctx.r3.u64=task;ctx.r6.u64=0;ctx.r7.u64=0;
  if(direct) {
    // The pad task completes near the pad centre, not near its target actor.
    // Use the native point-movement task for the visible final approach.
    const auto point=ctx.r1.u32+256;
    std::memcpy(base+point,base+player+48,12);
    ctx.r4.u64=point;ctx.r5.u64=1;sub_8219FF48(ctx,base);
  } else {
    // The actor-approach task gives up on a failed route. The native pad
    // navigation task supports persistent replanning (r6), including links
    // between navigation regions. Keep it alive until we reach the player.
    ctx.r4.u64=pad;ctx.r5.u64=0;ctx.r6.u64=1;ctx.r7.u64=0;ctx.r8.u64=0;
    sub_8219E750(ctx,base);
  }
  ctx.r3.u64=follow_reference;ctx.r4.u64=task;sub_82143128(ctx,base);
  ctx.r3.u64=ally+7912;ctx.r4.u64=task;sub_8217A530(ctx,base);
  follow_direct=direct;follow_destination=destination;follow_pad=pad;
  REXLOG_INFO("Coop ally: follow task {:08X} mode {} destination ({:.1f}, {:.1f}, {:.1f})",task,
              direct?"position":"route",destination.x,destination.y,destination.z);
}
void StopFollowing(PPCContext ctx,uint8_t* base) {
  if(Readable(ally,13040))FollowGait(ctx,base,false);
  if(!Readable(follow_reference,8))return;
  const auto task=REX_LOAD_U32(follow_reference+4);
  if(!Readable(task,36)||REX_LOAD_U32(task+12)>=2)return;
  const auto table=REX_LOAD_U32(task);
  if(!Readable(table,28))return;
  const auto fn=REX_LOAD_U32(table+24);
  if(fn<0x82100000||fn>=0x82D00000)return;
  ctx.r1.u32-=2048;ctx.r3.u64=task;
  REX_CALL_INDIRECT_FUNC(fn);
  ctx.r3.u64=ally+7912;ctx.r4.u64=task;sub_8217B4F0(ctx,base);
}
bool Near(uint8_t* base,float metres) {
  if(!Readable(player,60)||!Readable(ally,60))return false;
  const auto a=ActorPosition(base,player),b=ActorPosition(base,ally);
  const float d=(a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
  return std::isfinite(d)&&d<=metres*metres;
}
void SetHealth(PPCContext ctx,uint8_t* base,uint32_t actor,float value) {
  if(!Readable(actor,4))return;
  const auto table=REX_LOAD_U32(actor);
  if(!Readable(table,344))return;
  const auto fn=REX_LOAD_U32(table+340);
  if(fn<0x82100000||fn>=0x82D00000)return;
  restoring_health=true;
  ctx.r1.u32-=2048;ctx.r3.u64=actor;ctx.r5.u64=0;ctx.f1.f64=value;
  REX_CALL_INDIRECT_FUNC(fn);
  restoring_health=false;
}
void InterceptLethalHealth(PPCContext& ctx,uint8_t* base,bool is_player) {
  using pdz_coop::Partner;
  const auto actor=ctx.r3.u32;
  if(restoring_health||!Campaign(base)||!ally||!player||
     actor!=(is_player?player:ally)||ctx.r5.u8||!std::isfinite(ctx.f1.f64))return;
  if(rescue.Wiped())return;
  const auto who=is_player?Partner::Player:Partner::Ally;
  if(rescue.IsDown(who)) {ctx.f1.f64=.001;return;}
  if(ctx.f1.f64>0||!Readable(ally,13040)||!Readable(player,13040))return;
  const auto other=is_player?ally:player;
  if(Health(ctx,base,other)<=0)return;
  const bool can_rescue=rescue.Down(who);
  player_downed=rescue.IsDown(Partner::Player);
  rescue_status=is_player?2:1;
  // Keep the native entity alive so weapons, collision and its allocation
  // survive a rescue. Never resurrect a freed corpse or reset a mission.
  if(can_rescue)ctx.f1.f64=.001;
  REXLOG_INFO("Bot co-op: {} downed, team wiped={}",is_player?"player":"ally",rescue.Wiped());
}
void Spawn(PPCContext ctx,uint8_t* base) {
  uint32_t source=0;
  for(const auto& c:characters)if(c.address!=player&&Readable(c.setup,268)&&
      Readable(c.address,13040)&&REX_LOAD_U32(c.address+12)==33) {source=c.setup;break;}
  if(!source)return;
  REXLOG_INFO("Coop ally: preparing native template {:08X} for player {:08X}",source,player);
  const auto factory=REX_LOAD_U32(0x82D6B2B0+76);
  if(!Readable(factory,8))return;
  ctx.r1.u32-=2048;ctx.r3.u64=factory;ctx.r4.u64=33;sub_82513DD8(ctx,base);
  const auto size=ctx.r3.u32;
  if(size<268||size>4096||!Readable(source,size))return;
  attempted=true;
  owned_setup=rex::Runtime::instance()->memory()->SystemHeapAlloc(size+64,16);
  if(!owned_setup)return;
  std::memcpy(base+owned_setup,base+source,size);
  // Runtime clones are not registered XML assets. A named actor causes the
  // native constructor to look up and retain its nonexistent XML owner.
  // An anonymous setup takes the supported no-registration branch instead.
  constexpr char name[]="";
  std::memcpy(base+owned_setup+size,name,sizeof(name));
  REX_STORE_U32(owned_setup+52,owned_setup+size);
  follow_reference=owned_setup+size+32;
  std::memset(base+follow_reference,0,8);
  // The companion uses the primary player's selected weapon slots, including
  // category substitutions such as Laptop Gun instead of an assault rifle.
  auto profile_call=ctx;
  profile_call.r3.u64=REX_LOAD_U8(0x82D2CA58+20);
  sub_824FB718(profile_call,base);
  std::array<uint32_t,4> loadout{};
  if(Readable(profile_call.r3.u32,9460)) {
    for(unsigned i=0;i<loadout.size();++i) {
      const auto id=REX_LOAD_U32(profile_call.r3.u32+9444+i*4);
      if(id>0&&id<=31)loadout[i]=id;
    }
    for(unsigned i=0;i<2;++i) {
      REX_STORE_U32(owned_setup+184+i*12,loadout[i]);
      REX_STORE_U32(owned_setup+188+i*12,0);
      REX_STORE_U32(owned_setup+192+i*12,0);
    }
  }
  combat_script=REX_LOAD_U8(source+172);
  REX_STORE_U8(owned_setup+172,0); // Native inert script; navigation is queued separately.
  REX_STORE_U8(owned_setup+56,8); // Clear the copied enemy affiliation in construction.
  for(unsigned i=0;i<3;++i)REX_STORE_U32(owned_setup+4+i*4,REX_LOAD_U32(player+48+i*4));
  const float x=std::bit_cast<float>(REX_LOAD_U32(player+48));
  REX_STORE_U32(owned_setup+4,std::bit_cast<uint32_t>(x+120.f));
  spawning=true;
  REXLOG_INFO("Coop ally: creating native character from setup {:08X}, size {}",owned_setup,size);
  ctx.r3.u64=factory;ctx.r4.u64=33;ctx.r5.u64=owned_setup;ctx.r6.u64=1;ctx.r7.u64=1;
  sub_82513D00(ctx,base);
  spawning=false;
  ally=ctx.r3.u32;
  if(!Readable(ally,13040)){ally=0;REXLOG_WARN("Coop ally: native spawn failed");return;}
  std::memcpy(base+ally+128,base+player+128,6);
  for(unsigned i=2;i<loadout.size();++i)if(loadout[i]) {
    auto give=ctx;give.r3.u64=ally;give.r4.u64=loadout[i];
    give.r5.u64=0;give.r6.u64=0;give.r7.u64=0;give.r8.u64=0;
    sub_8253CCB8(give,base);
  }
  REXLOG_INFO("Coop ally: mirrored primary loadout [{}, {}, {}, {}]",
              loadout[0],loadout[1],loadout[2],loadout[3]);
  policy.Reset(Handle(player),Handle(ally));
  ally_full_health=std::max(1.f,Health(ctx,base,ally));
  REXLOG_INFO("Coop ally: spawned {:08X}, player {:08X}, native combat script {}",ally,player,combat_script);
}
#include "coop_sync_runtime.inl"
}

namespace pdz_coop {
void PollRescueInput(bool held) {use_held=held;}
bool PlayerDowned() {return player_downed.load();}
int RescueStatus() {return rescue_status.load();}
int SyncStatus() {return sync_status.load();}
bool AssistDataThief(PPCContext& ctx,uint8_t* base,uint32_t puzzle,float seconds) {
  if(!Campaign(base)||!Readable(puzzle,152)||!Readable(ally,13040)||
     rescue.IsDown(Partner::Player)||rescue.IsDown(Partner::Ally)||!Near(base,4.f)||
     Health(ctx,base,ally)<=0||!std::isfinite(seconds)||seconds<=0) {
    assisted_puzzle=0;assist_seconds=0;return false;
  }
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(REX_LOAD_U8(session)||pdz_surface::IntroCameraActive())return false;
  const auto owner=REX_LOAD_U32(puzzle+4);
  const auto type=REX_LOAD_U32(puzzle);
  if(!Readable(owner,1376)||REX_LOAD_U32(owner+92)!=player||
     !REX_LOAD_U32(owner+1312)||REX_LOAD_U32(puzzle+32)!=1||
     (type!=0x820CEB48 && type!=0x820CEC5C && type!=0x820CEBE0)) {
    assisted_puzzle=0;assist_seconds=0;return false;
  }
  if(assisted_puzzle!=puzzle){assisted_puzzle=puzzle;assist_seconds=0;}
  assist_seconds+=std::min(seconds,.1f);
  if(assist_seconds<3.f)return false;
  if(!Sees(ctx,base,ally,player)){assist_seconds=0;return false;}
  auto call=ctx;call.r1.u32-=2048;call.r3.u64=puzzle;
  // These are the native successful completion callbacks, not cancellation
  // or mission-objective writes. They notify only the active gadget target.
  if(type==0x820CEB48)sub_82637A80(call,base);
  else if(type==0x820CEC5C)sub_8263C478(call,base);
  else sub_826393F8(call,base);
  assisted_puzzle=0;assist_seconds=0;
  REXLOG_INFO("Bot co-op: completed active gadget task {:08X}",type);
  return true;
}
void SelectBotCoop(bool enabled) {
  rex::system::xam::ResetVirtualCoopInput();
  native_partner_selected=enabled&&REXCVAR_GET(pdz_coop_native_partner);
  rex::system::xam::SetVirtualCoopGuest(native_partner_selected);
  bot_coop_selected=enabled;
  if(!enabled){rescue.Reset();player_downed=false;rescue_status=0;use_held=false;
    sync_policy.Reset();sync_status=0;}
}
bool BotCoopSelected() { return bot_coop_selected; }
bool NativePartnerSelected() { return native_partner_selected; }
void CharacterCreated(uint8_t* base,uint32_t actor,uint32_t setup) {
  if(!Readable(actor,13040))return;
  for(auto& c:characters)if(!c.address||c.address==actor) {
    policy.Forget(Handle(actor));sync_policy.Forget(Handle(actor));c={actor,setup,++generation};
    if(spawning){ally=actor;if(Readable(player,136))std::memcpy(base+ally+128,base+player+128,6);}
    return;
  }
}
void CharacterDestroyed(uint32_t actor) {
  policy.Forget(Handle(actor));
  sync_policy.Forget(Handle(actor));
  for(auto& c:characters)if(c.address==actor)c={};
  if(ally==actor)ally=0; // No respawn loop after the companion dies.
}
void LevelEnded() {
  rex::system::xam::ResetVirtualCoopInput();
  rescue.Reset();player_downed=false;rescue_status=0;use_held=false;
  sync_policy.Reset();sync_status=0;sync_formation=sync_firing=false;
  sync_player_weapon=sync_weapon=sync_saved_weapon=0;sync_sample={};
  sync_movement=SyncMovement::None;
  assisted_puzzle=0;assist_seconds=0;
  characters={};policy.Reset(0,0);ally=player=0;attempted=false;
  if(owned_setup)rex::Runtime::instance()->memory()->SystemHeapFree(owned_setup);
  owned_setup=follow_reference=0;last_order=Order::Hold;next_tick={};
  next_follow={};follow_destination={};follow_direct=false;
  follow_pad=0;follow_gait=-1;
}
void Tick(PPCContext& ctx,uint8_t* base) {
  if(!Campaign(base)){if(sync_formation)EndSync(ctx,base);return;}
  const auto local=REX_LOAD_U32(0x82D2AD38);
  if(!Readable(local,13040)||REX_LOAD_U32(local+12)!=2)return;
  player=local;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(REX_LOAD_U8(session)||REX_LOAD_U8(player+0x16A3)||pdz_surface::IntroCameraActive()) {
    if(sync_formation||sync_policy.Target())EndSync(ctx,base);
    else {sync_policy.Reset();sync_sample={};}
    return;
  }
  if(attempted&&UpdateSync(ctx,base).active)return;
  const auto now=std::chrono::steady_clock::now();
  if(now<next_tick)return;
  next_tick=now+std::chrono::milliseconds(300);
  if(!attempted){Spawn(ctx,base);return;}
  if(rescue.Wiped() || (rescue.IsDown(Partner::Player)&&!Readable(ally,13040))) {
    // Let the normal mission-failure path run when nobody can provide aid.
    rescue.Reset();player_downed=false;rescue_status=0;
    SetHealth(ctx,base,player,0.f);
    return;
  }
  if(!Readable(ally,13040)||Health(ctx,base,ally)<=0)return;
  if(rescue.IsDown(Partner::Ally)||rescue.IsDown(Partner::Player)) {
    const bool ally_down=rescue.IsDown(Partner::Ally);
    if(last_order!=Order::Hold){StopFollowing(ctx,base);SetScript(ctx,base,0);last_order=Order::Hold;}
    const bool reachable=Near(base,2.f)&&Sees(ctx,base,ally,player);
    rescue_status=ally_down?1:(reachable?3:2);
    if(!ally_down&&!reachable)Follow(ctx,base);
    else StopFollowing(ctx,base);
    const auto who=ally_down?Partner::Ally:Partner::Player;
    if(rescue.Advance(who,reachable,ally_down?use_held.load():true,.3f)) {
      const auto actor=ally_down?ally:player;
      SetHealth(ctx,base,actor,ally_down?std::min(50.f,ally_full_health):50.f);
      player_downed=false;rescue_status=0;
      REXLOG_INFO("Bot co-op: revived {}",ally_down?"ally":"player");
    }
    return;
  }
  std::vector<AllyPolicy::Candidate> enemies;
  for(const auto& c:characters)if(c.address!=ally&&Readable(c.address,13040)&&policy.CanEngage(Handle(c.address)))
    enemies.push_back({Handle(c.address),ActorPosition(base,c.address),Health(ctx,base,c.address)>0,
                      HostileToPlayer(ctx,base,c.address),Sees(ctx,base,ally,c.address)});
  const auto decision=policy.Tick(ActorPosition(base,player),ActorPosition(base,ally),Health(ctx,base,player)>0,true,
                                  enemies.data(),unsigned(enemies.size()));
  if(decision.order!=last_order) {
    if(last_order==Order::Follow) {
      const auto task=REX_LOAD_U32(follow_reference+4);
      if(Readable(task,36)) {
        auto call=ctx;call.r1.u32-=2048;call.r3.u64=task;
        const auto fn=REX_LOAD_U32(REX_LOAD_U32(task)+24);
        // Stop the movement before removing the queued task.
        auto saved=ctx;ctx=call;REX_CALL_INDIRECT_FUNC(fn);ctx=saved;
        call.r3.u64=ally+7912;call.r4.u64=task;sub_8217B4F0(call,base);
      }
    }
    SetScript(ctx,base,decision.order==Order::Defend?combat_script:0);
    last_order=decision.order;
    REXLOG_INFO("Coop ally: order {} target {:08X}",int(last_order),uint32_t(decision.target));
  }
  if(decision.order==Order::Follow)Follow(ctx,base);
}

REX_EXTERN(__imp__sub_8216C780);
REX_HOOK_RAW(sub_8216C780) {
  if(ally&&Campaign(base)&&ctx.r3.u32==ally+6356&&!changing_script&&last_order!=pdz_coop::Order::Defend) {
    // Perception may request a combat/search script while we are following.
    // Dispose the uninstalled script through its native destructor.
    const auto instance=ctx.r4.u32;
    if(Readable(instance,4)) {
      const auto table=REX_LOAD_U32(instance);
      if(Readable(table,4)) {
        const auto fn=REX_LOAD_U32(table);
        if(fn>=0x82100000&&fn<0x82D00000) {
          const auto saved=ctx;ctx.r3.u64=instance;ctx.r4.u64=1;
          REX_CALL_INDIRECT_FUNC(fn);ctx=saved;
        }
      }
    }
    return;
  }
  __imp__sub_8216C780(ctx,base);
}
}

REX_EXTERN(__imp__sub_822343D8);
REX_HOOK_RAW(sub_822343D8) {
  if(ally&&Campaign(base)&&Readable(ctx.r3.u32,32)) {
    const auto weapon=REX_LOAD_U32(ctx.r3.u32+28);
    if(Readable(weapon,96)) {
      const auto shooter=REX_LOAD_U32(weapon+92);
      if(Target(base,shooter)==player&&HostileToPlayer(ctx,base,shooter)&&Sees(ctx,base,shooter,player)) {
        policy.SpottedPlayer(Handle(shooter),true);
        policy.FiredAt(Handle(shooter),Handle(player),true);
      }
    }
  }
  __imp__sub_822343D8(ctx,base);
}

REX_EXTERN(__imp__sub_82599C50);
REX_HOOK_RAW(sub_82599C50) {
  if(pdz_input::Supported()&&REXCVAR_GET(pdz_invincibility)&&!ctx.r5.u8&&std::isfinite(ctx.f1.f64)){
    const uint32_t camera=REX_LOAD_U32(0x82D68320);
    if(Readable(camera,12)&&REX_LOAD_U32(camera+8)==ctx.r3.u32){
      const float current=Health(ctx,base,ctx.r3.u32);
      if(current>0.f&&ctx.f1.f64<current)ctx.f1.f64=current;
    }
  }
  InterceptLethalHealth(ctx,base,true);
  __imp__sub_82599C50(ctx,base);
}
REX_EXTERN(__imp__sub_8251EE38);
REX_HOOK_RAW(sub_8251EE38) {
  InterceptLethalHealth(ctx,base,false);
  __imp__sub_8251EE38(ctx,base);
}
REX_EXTERN(__imp__sub_8263C588);
REX_HOOK_RAW(sub_8263C588) {
  if(!pdz_coop::AssistDataThief(ctx,base,ctx.r3.u32,float(ctx.f1.f64)))
    __imp__sub_8263C588(ctx,base);
}
REX_EXTERN(__imp__sub_82639498);
REX_HOOK_RAW(sub_82639498) {
  if(!pdz_coop::AssistDataThief(ctx,base,ctx.r3.u32,float(ctx.f1.f64)))
    __imp__sub_82639498(ctx,base);
}

REX_EXTERN(__imp__sub_825104D8);
REX_HOOK_RAW(sub_825104D8) {
  if(ally&&Campaign(base)) {
    const auto a=ctx.r3.u32,b=ctx.r4.u32;
    if((a==ally&&b!=player&&!policy.CanEngage(Handle(b))&&sync_policy.Target()!=Handle(b)&&
        !(sync_firing&&sync_shot_target==Handle(b)))||
       (b==ally&&a!=player&&!policy.EnemyMayTargetAlly(Handle(a)))) {ctx.r3.u64=1;return;}
  }
  __imp__sub_825104D8(ctx,base);
}

REX_EXTERN(__imp__sub_822339B0);
REX_HOOK_RAW(sub_822339B0) {
  if(pdz_klobb::HandleReloadSound(ctx,base))return;
  if(ally&&Campaign(base)&&Readable(ctx.r3.u32,32)&&Readable(ctx.r4.u32,6)&&
     REX_LOAD_U16(ctx.r4.u32+4)==1) {
    const auto weapon=REX_LOAD_U32(ctx.r3.u32+28);
    if(Readable(weapon,96)) {
      const auto shooter=REX_LOAD_U32(weapon+92);
      // Gate the native firing event before it consumes ammo or produces FX.
      // Other animation events (including reload/cleanup) still run normally.
      if((shooter==ally&&(rescue.IsDown(pdz_coop::Partner::Ally)||(sync_formation&&!sync_firing)))||
         (shooter==player&&rescue.IsDown(pdz_coop::Partner::Player))) {ctx.r3.u64=1;return;}
    }
  }
  __imp__sub_822339B0(ctx,base);
}

REX_EXTERN(__imp__sub_825787B8);
REX_HOOK_RAW(sub_825787B8) {
  const auto shooter=ctx.r4.u32;
  if(!ally||!Campaign(base)){__imp__sub_825787B8(ctx,base);return;}
  if((shooter==ally&&rescue.IsDown(pdz_coop::Partner::Ally))||
     (shooter==player&&rescue.IsDown(pdz_coop::Partner::Player)))return;
  if(sync_firing&&shooter==ally&&ctx.r3.u32==sync_weapon) {
    const auto oldStack=ctx.r1.u64;ctx.r1.u32-=2048;
    WritePoint(base,ctx.r1.u32+256,sync_shot_origin);
    WritePoint(base,ctx.r1.u32+272,sync_shot_head);
    ctx.r5.u64=ctx.r1.u32+256;ctx.r6.u64=ctx.r1.u32+272;
    ctx.r7.u64=0; // Exact head segment, no random directional spread.
    // r8 retains the native suppressed/unsuppressed shot mode.
    __imp__sub_825787B8(ctx,base);ctx.r1.u64=oldStack;
    sync_discharged=true;return;
  }
  if(shooter==ally&&sync_formation)return;
  const auto saved=ctx;
  const auto plan=shooter==player?UpdateSync(ctx,base,true):pdz_coop::SyncDecision{};
  __imp__sub_825787B8(ctx,base);
  // Both bullets run in the same simulation event. The plan is captured before
  // the player's hit can remove their target and invalidate the group lock.
  if(plan.fire)FireSynchronized(saved,base,plan);
}
