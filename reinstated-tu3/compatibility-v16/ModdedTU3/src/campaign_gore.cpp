#include "campaign_damage.h"
#include "generated/default/pdzstocktu3_init.h"
#include "campaign_gore.h"
#include "gore_policy.h"
#include "death_voice.h"
#include "blood_decals.h"
#include "mousehook.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include "game_cvars.h"
#include "mp_progression_runtime.h"

REXCVAR_DEFINE_BOOL(pdz_campaign_gore, true, "Gameplay", "Weapon-specific dismemberment in single-player missions only");
namespace {
std::array<std::atomic<uint32_t>,2500> removed_heads{};
std::array<std::atomic<uint32_t>,2500> headless_bodies{};
std::array<std::atomic<unsigned>,2500> removed_parts{};
struct GoreEmitter { uint32_t object=0,definition=0; };
std::array<GoreEmitter,64> gore_emitters{};
bool GoreParticles(uint8_t* base,uint32_t object){
  return std::any_of(gore_emitters.begin(),gore_emitters.end(),[&](const auto& e){
    return e.object==object && object && REX_LOAD_U32(object+156)==e.definition;
  });
}
bool Readable(uint32_t a,uint32_t n){
  auto* rt=rex::Runtime::instance();
  if(!rt||!a||uint64_t(a)+n>0x100000000ull)return false;
  for(uint64_t p=a;p<uint64_t(a)+n;){
    auto* h=rt->memory()->LookupHeap(uint32_t(p));uint32_t flags=0;
    if(!h||!h->QueryProtect(uint32_t(p),&flags)||!(flags&rex::memory::kMemoryProtectRead))return false;
    p=(p/h->page_size()+1)*h->page_size();
  }return true;
}
int ModelSlot(uint32_t model){
  if(model<0x82E75160 || (model-0x82E75160)%512)return -1;
  const auto slot=(model-0x82E75160)/512;
  return slot<removed_heads.size()?int(slot):-1;
}
uint32_t WeaponResource(PPCContext call,uint8_t* base,const char* name){
  call.r1.u32-=1024;
  if(!Readable(call.r1.u32,1024))return 0;
  const auto text=call.r1.u32+256,out=call.r1.u32+128;
  std::memcpy(base+text,name,std::strlen(name)+1);
  REX_STORE_U32(out,0);
  // Resolve the loaded CAFF resource and unwrap its header. 825E4798 is
  // specifically an emitter loader: its string fields overlap gun floats.
  call.r3.u64=text;call.r4.u64=out;
  sub_821F4148(call,base);
  const auto resource=REX_LOAD_U32(out);
  if(!Readable(resource,8))return 0;
  call.r3.u64=resource;sub_82657700(call,base);
  return Readable(call.r3.u32,1148)?call.r3.u32:0;
}
uint32_t EmitterResource(PPCContext call,uint8_t* base,const char* name){
  const auto names=REX_LOAD_U32(0x82D6BFD8+72);
  call.r1.u32-=1024;const auto text=call.r1.u32+256;
  if(!Readable(names,24)||!Readable(call.r1.u32,1024))return 0;
  std::memcpy(base+text,name,std::strlen(name)+1);
  call.r3.u64=names+20;call.r4.u64=text;call.r5.u64=0;
  sub_825F41B0(call,base);return call.r3.u32;
}
float Health(PPCContext ctx,uint8_t* base,uint32_t owner){
  if(!Readable(owner,4))return 1;
  const auto table=REX_LOAD_U32(owner);
  if(!Readable(table,336))return 1;
  const auto fn=REX_LOAD_U32(table+332);
  if(fn<0x82100000||fn>=0x82D00000)return 1;
  ctx.r1.u32-=1024;
  if(!Readable(ctx.r1.u32,1024))return 1;
  ctx.r3.u64=owner;ctx.r4.u64=0;
  REX_CALL_INDIRECT_FUNC(fn);
  const auto health=float(ctx.f1.f64);
  return std::isfinite(health)?health:1.f;
}
using Vector=std::array<uint32_t,3>;
void Burst(PPCContext call,uint8_t* base,uint32_t emitter,const Vector& position,const Vector& normal,bool persistent=false){
  if(!Readable(emitter,436))return;
  if(persistent && std::none_of(gore_emitters.begin(),gore_emitters.end(),[](const auto& e){return !e.object;}))return;
  call.r1.u32-=1024;if(!Readable(call.r1.u32,1024))return;
  // Same emitter constructor/ABI used by the native impact effects. The
  // engine owns particle updates, collision and cleanup.
  REX_STORE_U32(call.r1.u32+84,0xffffffffu);
  REX_STORE_U8(call.r1.u32+95,0);REX_STORE_U8(call.r1.u32+103,0);
  for(unsigned i=0;i<3;++i){REX_STORE_U32(call.r1.u32+256+i*4,position[i]);REX_STORE_U32(call.r1.u32+272+i*4,normal[i]);}
  call.r3.u64=0;call.r4.u64=emitter;call.r5.u64=call.r1.u32+256;
  call.r6.u64=call.r1.u32+272;call.r7.u64=0;call.r8.u64=0;call.r9.u64=0;call.r10.u64=1;
  // Collision must be enabled during allocation as well as particle updates:
  // the allocator chooses a different native update list for colliding effects.
  const auto collision=REX_LOAD_U8(emitter+372);
  if(persistent)REX_STORE_U8(emitter+372,1);
  sub_82611C10(call,base);
  REX_STORE_U8(emitter+372,collision);
  if(persistent && Readable(call.r3.u32,200)){
    auto slot=std::find_if(gore_emitters.begin(),gore_emitters.end(),[](const auto& e){return !e.object;});
    if(slot!=gore_emitters.end())*slot={call.r3.u32,emitter};
  }
}
// Restore shared emitter data immediately after each native call. Ordinary
// birds and all other particle effects retain their retail settings.
struct FragmentSettings {
  uint8_t* base;uint32_t definition=0;std::array<uint32_t,3> words{};uint8_t collision=0,mode=0;
  FragmentSettings(uint8_t* b,uint32_t object):base(b){
    if(!pdz_gore::Campaign(base)||!Readable(object,200)||!GoreParticles(base,object))return;
    definition=REX_LOAD_U32(object+156);
    if(!Readable(definition,436)){definition=0;return;}
    words={REX_LOAD_U32(definition+292),REX_LOAD_U32(definition+296),REX_LOAD_U32(definition+376)};
    collision=REX_LOAD_U8(definition+372);mode=REX_LOAD_U8(definition+373);
    const float seconds=float(std::clamp(REXCVAR_GET(pdz_gore_decal_minutes),1,20)*60);
    REX_STORE_U32(definition+292,std::bit_cast<uint32_t>(seconds));
    REX_STORE_U32(definition+296,std::bit_cast<uint32_t>(seconds));
    REX_STORE_U8(definition+372,1);REX_STORE_U8(definition+373,1);
    REX_STORE_U32(definition+376,std::bit_cast<uint32_t>(.25f));
  }
  ~FragmentSettings(){if(!definition)return;
    REX_STORE_U32(definition+292,words[0]);REX_STORE_U32(definition+296,words[1]);
    REX_STORE_U32(definition+376,words[2]);
    REX_STORE_U8(definition+372,collision);REX_STORE_U8(definition+373,mode);
  }
};
struct HeadPalette {
  struct Saved {uint32_t address;std::array<uint8_t,64> bytes;};
  uint8_t* base;std::vector<Saved> saved;
  explicit HeadPalette(uint8_t* b,uint32_t model,uint32_t graph,uint32_t frameState):base(b){
    if(!pdz_gore::Campaign(base))return;
    // Native rendering has pinned this one model. Do not edit all models, or
    // both buffers: the other buffer belongs to the simulation thread and an
    // unreferenced model can be retired/reused while a draw is in progress.
    const int slot=ModelSlot(model);
    if(slot<0||!Readable(model,68))return;
    const auto definition=headless_bodies[slot].load();
    if(!definition||REX_LOAD_U32(model)!=definition||!Readable(definition,16)||
       REX_LOAD_U32(definition+4)!=graph)return;
    unsigned frame=2;
    for(unsigned i=0;i<2;++i)if(REX_LOAD_U32(model+32+i*4)==frameState)frame=i;
    if(frame>1)return;
    const auto skeleton=REX_LOAD_U32(definition+12);if(!Readable(skeleton,56))return;
    const auto count=REX_LOAD_U32(skeleton+48),bind=REX_LOAD_U32(skeleton+52);
    if(count<=17||count>256||count!=REX_LOAD_U32(definition+8)||!Readable(bind,count*36))return;
    {
      for(unsigned part=1;part<=5;++part){
        if(!(removed_parts[slot]&gore_policy::Bit(gore_policy::Part(part))))continue;
        const auto root=gore_policy::Root(gore_policy::Part(part));
        if(root>=count)continue;
      {
        const auto palette=REX_LOAD_U32(model+60+frame*4);
        if(!Readable(palette,count*64))continue;
        const auto head=palette+root*64;
        auto f=[&](uint32_t a){return std::bit_cast<float>(REX_LOAD_U32(a));};
        std::array<float,3> centre{};
        for(unsigned axis=0;axis<3;++axis){
          centre[axis]=f(head+48+axis*4);
          for(unsigned k=0;k<3;++k)centre[axis]+=f(head+k*16+axis*4)*f(bind+root*36+12+k*4);
        }
        if(!std::all_of(centre.begin(),centre.end(),[](float v){return std::isfinite(v);}))continue;
        for(unsigned joint=0;joint<count;++joint){
          unsigned ancestor=joint;
          for(unsigned depth=0;ancestor!=root&&ancestor<count&&depth<count;++depth){
            const auto parent=REX_LOAD_U16(bind+ancestor*36+24);
            if(parent==ancestor)break;ancestor=parent;
          }
          if(ancestor!=root)continue;
          const auto at=palette+joint*64;Saved copy{at,{}};
          std::memcpy(copy.bytes.data(),base+at,64);saved.push_back(copy);
          // Collapse only the selected limb and its child skin matrices during drawing.
          // Body/ragdoll simulation and shared mesh assets remain untouched.
          for(unsigned k=0;k<3;++k)for(unsigned axis=0;axis<3;++axis)REX_STORE_U32(at+k*16+axis*4,0);
          for(unsigned axis=0;axis<3;++axis)REX_STORE_U32(at+48+axis*4,std::bit_cast<uint32_t>(centre[axis]));
        }
      }
      }
    }
  }
  ~HeadPalette(){for(auto it=saved.rbegin();it!=saved.rend();++it)std::memcpy(base+it->address,it->bytes.data(),64);}
};
}
namespace pdz_gore {
bool Campaign(uint8_t* base){
  if(!pdz_input::Supported())return false;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  return Readable(session,44)&&REX_LOAD_U32(session+40)==0;
}
void Reset(){for(auto& v:removed_heads)v=0;for(auto& v:headless_bodies)v=0;for(auto& v:removed_parts)v=0;gore_emitters={};pdz_death_voice::Reset();}
unsigned RemovedParts(uint8_t* base,uint32_t model){
  const auto slot=ModelSlot(model);
  return slot>=0&&Readable(model,4)&&headless_bodies[slot]==REX_LOAD_U32(model)?removed_parts[slot].load():0;
}
void AdoptParts(uint8_t* base,uint32_t model,uint32_t head,unsigned mask){
  const auto slot=ModelSlot(model);if(!mask||slot<0||!Readable(model,4))return;
  headless_bodies[slot]=REX_LOAD_U32(model);removed_parts[slot]|=mask&31;
  const auto h=ModelSlot(head);
  if((mask&gore_policy::Bit(gore_policy::Part::Head))&&h>=0&&Readable(head,4)){
    removed_heads[h]=REX_LOAD_U32(head);HideHead(base,head);
  }
}
void HideHead(uint8_t* base,uint32_t model){
  const auto slot=ModelSlot(model);
  if(slot<0||!removed_heads[slot]||!Campaign(base)||!Readable(model,4))return;
  if(REX_LOAD_U32(model)!=removed_heads[slot]){removed_heads[slot]=0;return;}
  const auto entry=0x82D73900u+uint32_t(slot)*8;
  if(Readable(entry,4))REX_STORE_U32(entry,REX_LOAD_U32(entry)&~0xffffu);
}
}
REX_EXTERN(__imp__sub_82411048);
REX_HOOK_RAW(sub_82411048){
  __imp__sub_82411048(ctx,base);
  // Pool addresses are reused. A new allocation must never inherit gore.
  pdz_death_voice::Forget(ctx.r3.u32);
  const auto slot=ModelSlot(ctx.r3.u32);if(slot>=0){removed_heads[slot]=0;headless_bodies[slot]=0;removed_parts[slot]=0;}
}
REX_EXTERN(__imp__sub_82549040);
REX_EXTERN(__imp__sub_82559AA0);
static void WeaponGoreHit(PPCContext& ctx,uint8_t* base,bool corpse){
  const auto owner=ctx.r3.u32,weapon=ctx.r4.u32,shooter=ctx.r5.u32,record=ctx.r6.u32;
  // Complete after dismemberment, including early returns from its optional path.
  struct DeathSound {
    PPCContext context;uint8_t* base;pdz_death_voice::Shot shot{};uint32_t partsBefore=0;
    ~DeathSound(){if(shot.actor)pdz_death_voice::End(context,base,shot,Health(context,base,shot.actor),
      (pdz_gore::RemovedParts(base,shot.model)&1)!=0,
      pdz_gore::RemovedParts(base,shot.model)!=partsBefore);}
  } deathSound{ctx,base};
  if(!corpse&&pdz_input::Supported()&&Readable(owner,5284)&&Readable(record,56)&&Readable(weapon,864)){
    const auto region=REX_LOAD_U32(record+52),type=REX_LOAD_U32(owner+12);
    const auto local=REX_LOAD_U32(0x82E36228);
    if((type==33||type==42)&&owner!=shooter&&
       (!Readable(local,968)||REX_LOAD_U32(local+964)!=owner)&&
       REX_LOAD_U32(weapon+12)==4&&REX_LOAD_U32(weapon+92)==shooter&&Readable(shooter,60)){
      auto relation=ctx;relation.r1.u32-=1024;
      if(Readable(relation.r1.u32,1024)){
        relation.r3.u64=owner;relation.r4.u64=shooter;sub_8251EBD0(relation,base);
        const float before=Health(ctx,base,owner);
        if(!relation.r3.u8&&before>0){
          deathSound.shot={owner,REX_LOAD_U32(owner+72),REX_LOAD_U32(owner+5280),region,before};
          deathSound.partsBefore=pdz_gore::RemovedParts(base,deathSound.shot.model);
          pdz_death_voice::PrepareKnockout(ctx,base,owner);
          pdz_death_voice::Begin(deathSound.shot);
        }
      }
    }
  }
  // Only roster participants in Combat Arena are eligible. Campaign NPCs,
  // props and repeated corpse impacts never enter the career accounting path.
  bool mpVictim=false,mpShooter=false;
  uint32_t localActor=0,careerWeapon=0,careerPart=0;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  if(!corpse&&pdz_input::Supported()&&Readable(session,44)&&
     (REX_LOAD_U32(session+40)==2||REX_LOAD_U32(session+40)==3)) {
    std::array<uint32_t,32> seen{};unsigned count=0;
    for(auto p=REX_LOAD_U32(0x82E34224);p&&count<seen.size()&&Readable(p,968);p=REX_LOAD_U32(p+4)) {
      if(std::find(seen.begin(),seen.begin()+count,p)!=seen.begin()+count)break;
      seen[count++]=p;const auto actor=REX_LOAD_U32(p+964);
      if(count==1)localActor=actor;
      if(actor==owner)mpVictim=true;if(actor==shooter)mpShooter=true;
    }
    if(Readable(weapon,864)&&REX_LOAD_U32(weapon+12)==4&&REX_LOAD_U32(weapon+92)==shooter) {
      const auto definition=REX_LOAD_U32(weapon+860);
      if(Readable(definition,1148))careerWeapon=REX_LOAD_U32(definition+1144);
    }
    if(Readable(record,56))careerPart=REX_LOAD_U32(record+52);
  }
  const bool careerHit=mpVictim&&(owner==localActor||(mpShooter&&shooter==localActor));
  const float healthBefore=careerHit?Health(ctx,base,owner):0;
  const auto careerContext=ctx;
  uint32_t head=0,body=0;bool blade=false;gore_policy::Part removed=gore_policy::Part::None;
  auto saved=ctx;Vector position{},normal{};
  if(pdz_gore::Campaign(base)&&REXCVAR_GET(pdz_campaign_gore)&&
     Readable(owner,corpse?1044:4112)&&Readable(weapon,864)&&Readable(record,112)){
    const auto local=REX_LOAD_U32(0x82E36228);
    const auto part=REX_LOAD_U32(record+52);
    const auto type=REX_LOAD_U32(owner+12);
    if((corpse?type==43:(type==33||type==42))&&Readable(local,968)&&REX_LOAD_U32(local+964)==shooter&&
       owner!=shooter&&REX_LOAD_U32(weapon+12)==4&&REX_LOAD_U32(weapon+92)==shooter&&Readable(shooter,60)){
      const auto definition=REX_LOAD_U32(weapon+860);
      const auto weaponId=Readable(definition,1148)?REX_LOAD_U32(definition+1144):~0u;
      blade=weaponId==27;
      static thread_local std::mt19937 rng{std::random_device{}()};
      const auto candidate=gore_policy::ImpactPart(weaponId,part,blade?std::uniform_int_distribution<unsigned>(0,99)(rng):0,blade?rng():0);
      float distanceSquared=0;
      for(unsigned axis=0;axis<3;++axis){
        const float delta=std::bit_cast<float>(REX_LOAD_U32(owner+48+axis*4))-
          std::bit_cast<float>(REX_LOAD_U32(shooter+48+axis*4));
        distanceSquared+=delta*delta;
      }
      if(Readable(definition,1148)&&gore_policy::Allowed(REX_LOAD_U32(definition+1144),candidate,distanceSquared)){
        auto team=ctx;
        if(team.r1.u32>=1024&&Readable(team.r1.u32-1024,1024)){
          team.r1.u32-=1024;team.r3.u64=owner;team.r4.u64=shooter;sub_8251EBD0(team,base);
          if(!team.r3.u8){
            removed=candidate;head=REX_LOAD_U32(owner+(corpse?1008:4104));body=REX_LOAD_U32(owner+72);
          }
        }
      }
    }
  }
  if(body){
    auto point=ctx;point.r1.u32-=1024;
    if(!Readable(point.r1.u32,1024))body=0;
    else {
      point.r3.u64=owner;point.r4.u64=record+68;point.r5.u64=point.r1.u32+256;
      sub_82518E60(point,base);
      for(unsigned i=0;i<3;++i)position[i]=REX_LOAD_U32(point.r1.u32+256+i*4);
      point.r3.u64=owner;point.r4.u64=record+92;point.r5.u64=point.r1.u32+256;
      sub_82518F08(point,base);
      for(unsigned i=0;i<3;++i)normal[i]=REX_LOAD_U32(point.r1.u32+256+i*4);
      for(const auto bits:position)if(!std::isfinite(std::bit_cast<float>(bits)))body=0;
      for(const auto bits:normal)if(!std::isfinite(std::bit_cast<float>(bits)))body=0;
    }
  }
  if(body&&pdz_death_voice::KnockedOut(base,owner)) {
    pdz_death_voice::PrepareKnockout(saved,base,owner);
    if(!deathSound.shot.actor) {
      deathSound.shot={owner,body,0,REX_LOAD_U32(record+52),0};
      deathSound.partsBefore=pdz_gore::RemovedParts(base,body);
      pdz_death_voice::Begin(deathSound.shot);
    }
  }
  if(corpse)__imp__sub_82559AA0(ctx,base);
  else pdz_campaign::BulletDamage(ctx,base);
  if(careerHit&&healthBefore>0&&Health(careerContext,base,owner)<=0) {
    pdz_progression::Event event;
    if(owner==localActor)event.deaths=1;
    if(mpShooter&&shooter==localActor&&owner!=shooter) {
      event.kills=1;event.xp=25;event.weapon=careerWeapon;
      if(careerWeapon==37||careerWeapon==38)event.counters[unsigned(pdz_progression::Counter::SniperKills)]=1;
      if(careerPart==1||careerPart==2)event.counters[unsigned(pdz_progression::Counter::Headshots)]=1;
    }
    pdz_progression::career.Record(0,event);
  }
  const auto slot=ModelSlot(head);
  const auto body_slot=ModelSlot(body);
  const auto bit=gore_policy::Bit(removed);
  if(body_slot<0||!Readable(body,68)||!bit||(removed_parts[body_slot]&bit)||
     (!corpse&&Health(saved,base,owner)>0&&!blade&&!pdz_death_voice::KnockedOut(base,owner)))return;
  // Do not open a hole unless the native attached cap was allocated first.
  if(!pdz_blood::Cap(saved,base,owner,gore_policy::Root(removed),gore_policy::Radius(removed)))return;
  headless_bodies[body_slot]=REX_LOAD_U32(body);removed_parts[body_slot]|=bit;
  if(removed==gore_policy::Part::Head&&slot>=0&&Readable(head,4)){
    removed_heads[slot]=REX_LOAD_U32(head);pdz_gore::HideHead(base,head);
  }
  // Reuse the bird's blood and bits without its feather emitter. The common
  // small-animal impact resource covers missions without the pigeon asset.
  auto bits=EmitterResource(saved,base,"aid_xml_emitter_effect_explosionpigeonbits");
  if(!bits)bits=EmitterResource(saved,base,"aid_xml_emitter_impact_smallanimalgibblets");
  const auto blood=EmitterResource(saved,base,"aid_xml_emitter_effect_explosionpigeonblood");
  for(unsigned i=0;i<6;++i)Burst(saved,base,bits,position,normal,true);
  Burst(saved,base,blood,position,normal);
  auto hit=saved;hit.r3.u64=owner;hit.r4.u64=record+68;hit.r5.u64=record+92;hit.r6.u64=record+40;
  pdz_blood::Hit(hit,base,true);
  REXLOG_INFO("Campaign dismemberment: victim={:08X} part={} bits={:08X} blood={:08X}",owner,unsigned(removed),bits,blood);
}
// Preserve the separate native corpse impact path and its ragdoll impulses.
REX_HOOK_RAW(sub_82549040){WeaponGoreHit(ctx,base,false);}
void pdz_gore::CorpseHit(PPCContext& ctx,uint8_t* base){WeaponGoreHit(ctx,base,true);}
REX_EXTERN(__imp__sub_82711538);
void pdz_gore::RenderModel(PPCContext& ctx,uint8_t* base){
  uint32_t model=0;
  // These callers pass the currently pinned scenegraph in r30, except the
  // deferred list at 82412338 which passes a pointer to its list entry.
  switch(uint32_t(ctx.lr)) {
    case 0x824120A8: case 0x824120F0: case 0x82412624: case 0x824127E8:
      model=ctx.r30.u32;break;
    case 0x82412338:
      if(Readable(ctx.r30.u32,4))model=REX_LOAD_U32(ctx.r30.u32);break;
  }
  HeadPalette heads(base,model,ctx.r3.u32,ctx.r6.u32);
  __imp__sub_82711538(ctx,base);
}
REX_EXTERN(__imp__sub_82611850);
REX_HOOK_RAW(sub_82611850){
  __imp__sub_82611850(ctx,base);
  for(auto& e:gore_emitters)if(e.object==ctx.r3.u32)e={};
}
REX_EXTERN(__imp__sub_82410910);
REX_HOOK_RAW(sub_82410910){
  pdz_death_voice::Forget(ctx.r3.u32);
  const auto slot=ModelSlot(ctx.r3.u32);
  if(slot>=0){removed_heads[slot]=0;headless_bodies[slot]=0;removed_parts[slot]=0;}
  __imp__sub_82410910(ctx,base);
}
REX_EXTERN(__imp__sub_82613908);
REX_HOOK_RAW(sub_82613908){FragmentSettings settings(base,ctx.r3.u32);__imp__sub_82613908(ctx,base);}
REX_EXTERN(__imp__sub_82615258);
REX_HOOK_RAW(sub_82615258){
  const auto block=ctx.r3.u32;
  FragmentSettings settings(base,Readable(block,17424)?REX_LOAD_U32(block+17420):0);
  __imp__sub_82615258(ctx,base);
}

