// Included by campaign_rules.cpp, sharing its validated guest-memory helpers.
REX_EXTERN(__imp__sub_82588040);
namespace pdz_custom {
namespace {
struct Reaction {uint32_t target{};float first{},last{};};
std::map<uint32_t,Reaction> reactions;
double Percent(double value) {return std::isfinite(value)?std::clamp(value,10.,1000.)/100.:1.;}
uint32_t Player(uint8_t* base) {
  const auto owner=REX_LOAD_U32(0x82E36228);
  return Readable(owner,968)?REX_LOAD_U32(owner+964):0;
}
bool Enemy(PPCContext call,uint8_t* base,uint32_t actor) {
  if(!Active(base)||!Readable(actor,13040)||REX_LOAD_U32(actor+12)!=33)return false;
  const auto player=Player(base);
  if(!player||player==actor||call.r1.u32<2048||!Readable(call.r1.u32-2048,2048))return false;
  call.r1.u32-=2048;call.r3.u64=actor;call.r4.u64=player;
  sub_8251EBD0(call,base);
  return !call.r3.u8;
}
}
bool Active(uint8_t* base) {
  if(!pdz_input::Supported()||!Unlocked()||!REXCVAR_GET(pdz_custom_mode))return false;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  return Readable(session,44)&&REX_LOAD_U32(session+40)==0;
}
void ScalePlayerDamage(PPCContext& ctx,uint8_t* base) {
  if(!LocalPlayer(base,ctx.r3.u32)||!Enemy(ctx,base,ctx.r4.u32))return;
  const double damage=Percent(REXCVAR_GET(pdz_enemy_damage));
  if(std::isfinite(ctx.f1.f64)&&ctx.f1.f64>0)ctx.f1.f64*=damage;
  if(std::isfinite(ctx.f2.f64)&&ctx.f2.f64>0)ctx.f2.f64*=damage;
}
void Forget(unsigned actor){reactions.erase(actor);}
void Reset(){reactions.clear();}
bool HoldFire(PPCContext& ctx,uint8_t* base,unsigned actor) {
  if(!Enemy(ctx,base,actor)){Forget(actor);return false;}
  const auto priority=REX_LOAD_U32(actor+12652);
  const auto target=priority?priority:REX_LOAD_U32(actor+12628);
  if(target!=Player(base)){Forget(actor);return false;}
  // Native simulation clock, also used by the enemy reaction task. Pausing
  // does not advance this clock; recycled actors are removed by Destroyed.
  const float now=ReadFloat(base,0x82E2F334);
  if(!std::isfinite(now))return false;
  auto [it,inserted]=reactions.try_emplace(actor,Reaction{target,now,now});
  auto& state=it->second;
  if(state.target!=target||now<state.last||now-state.last>float(2.+1./Percent(REXCVAR_GET(pdz_enemy_reaction))))state={target,now,now};
  state.last=now;
  return now-state.first<1./Percent(REXCVAR_GET(pdz_enemy_reaction));
}
void Fire(PPCContext& ctx,uint8_t* base) {
  const auto actor=ctx.r4.u32;
  if(!Enemy(ctx,base,actor)){__imp__sub_82588040(ctx,base);return;}
  if(HoldFire(ctx,base,actor)){ctx.r3.u64=0;return;}
  const auto target=Player(base);
  const auto priority=REX_LOAD_U32(actor+12652);
  if((priority?priority:REX_LOAD_U32(actor+12628))!=target){__imp__sub_82588040(ctx,base);return;}
  const double accuracy=Percent(REXCVAR_GET(pdz_enemy_accuracy));
  if(accuracy==1.||!Readable(target,60)||!Readable(ctx.r5.u32,12)||
     !Readable(ctx.r6.u32,12)||ctx.r1.u32<2048||!Readable(ctx.r1.u32-2048,2048)) {
    __imp__sub_82588040(ctx,base);return;
  }
  // Scale native aiming error around the target's torso, at the original
  // ray length. Preserve shot origin, weapon, ammunition and native hit tests.
  double origin[3],direction[3],ideal[3];double distance2=0,ideal2=0;
  for(unsigned i=0;i<3;++i){
    origin[i]=ReadFloat(base,ctx.r5.u32+i*4);
    direction[i]=ReadFloat(base,ctx.r6.u32+i*4)-origin[i];
    ideal[i]=ReadFloat(base,target+48+i*4)+(i==1?100.:0.)-origin[i];
    distance2+=direction[i]*direction[i];ideal2+=ideal[i]*ideal[i];
  }
  if(!std::isfinite(distance2)||!std::isfinite(ideal2)||distance2<1||ideal2<1){__imp__sub_82588040(ctx,base);return;}
  const auto stack=ctx.r1.u64;ctx.r1.u32-=2048;const auto endpoint=ctx.r1.u32+256;
  const double ratio=std::sqrt(distance2/ideal2);
  for(unsigned i=0;i<3;++i){const double centre=ideal[i]*ratio;
    StoreFloat(base,endpoint+i*4,float(origin[i]+centre+(direction[i]-centre)/accuracy));}
  ctx.r6.u64=endpoint;__imp__sub_82588040(ctx,base);ctx.r1.u64=stack;
}
}

REX_EXTERN(__imp__sub_82546BC8);
REX_HOOK_RAW(sub_82546BC8) {
  // NPC damage virtual: f1/f2 are the health/armour damage components.
  // Scaling damage gives the requested effective health without changing
  // scripted health setters, regeneration, or boss phase thresholds.
  if(pdz_custom::Enemy(ctx,base,ctx.r3.u32)){
    const double health=pdz_custom::Percent(REXCVAR_GET(pdz_enemy_health));
    if(std::isfinite(ctx.f1.f64)&&ctx.f1.f64>0)ctx.f1.f64/=health;
    if(std::isfinite(ctx.f2.f64)&&ctx.f2.f64>0)ctx.f2.f64/=health;
  }
  __imp__sub_82546BC8(ctx,base);
}
