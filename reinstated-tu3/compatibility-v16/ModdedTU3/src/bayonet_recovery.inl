// Included by weapon_rules.cpp: reuse its guarded guest-memory access.
namespace pdz_bayonet {
struct Blade {
  uint32_t object{}, weapon{}, player{}, table{}, model{};
  std::chrono::steady_clock::time_point created{};
};
std::array<Blade,32> blades{};
thread_local uint32_t firing_weapon=0, firing_player=0;
void Forget(uint32_t object) {
  for(auto& blade:blades)
    if(blade.object==object||blade.weapon==object||blade.player==object)blade={};
}
void Created(uint8_t* base,uint32_t object) {
  if(!firing_weapon||!Readable(object,100))return;
  for(auto& blade:blades)if(!blade.object){
    blade={object,firing_weapon,firing_player,REX_LOAD_U32(object),
           REX_LOAD_U32(object+72),std::chrono::steady_clock::now()};
    REXLOG_INFO("KSI bayonet: tracked blade {:08X} weapon {:08X}",object,firing_weapon);
    break;
  }
}
void Tick(PPCContext& original,uint8_t* base) {
  if(!pdz_input::Supported())return;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  if(!Readable(session,44)||REX_LOAD_U8(session)||REX_LOAD_U32(session+40)!=0)return;
  const auto now=std::chrono::steady_clock::now();
  for(auto& blade:blades){
    if(!blade.object)continue;
    const auto b=blade;
    if(!Readable(b.object,100)||REX_LOAD_U32(b.object)!=b.table||
       REX_LOAD_U32(b.object+72)!=b.model||!Readable(b.weapon,1324)||
       !Readable(b.player,6584)||REX_LOAD_U32(b.weapon+92)!=b.player){blade={};continue;}
    if(!LocalPlayer(base,b.player)||now-b.created<std::chrono::seconds(1))continue;
    const auto definition=REX_LOAD_U32(b.weapon+860),action=REX_LOAD_U32(b.weapon+1280);
    if(!Readable(definition,1148)||REX_LOAD_U32(definition+1144)!=1||
       !Readable(action,1308)||REX_LOAD_U32(action)!=0x82097498||
       REX_LOAD_U32(action+28)!=b.weapon||REX_LOAD_U8(action+1300))continue;
    const auto health=REX_LOAD_U32(b.player+6560);
    if(!Readable(health,984)||!(std::bit_cast<float>(REX_LOAD_U32(health+980))>0.f))continue;
    auto value=[&](uint32_t p){return std::bit_cast<float>(REX_LOAD_U32(p));};
    float distance=0;
    for(unsigned i=0;i<3;++i){
      const float delta=value(b.object+48+4*i)-value(b.player+48+4*i);
      distance+=delta*delta;
    }
    if(!std::isfinite(distance)||distance>125.f*125.f)continue;
    const auto animations=REX_LOAD_U32(b.weapon+1248);
    if(!Readable(animations,8))continue;
    const auto secondary=REX_LOAD_U32(animations+4),inventory=REX_LOAD_U32(b.weapon+1100);
    if(!Readable(secondary,200))continue;
    const auto ammo=inventory?inventory+2*(26+REX_LOAD_U8(secondary+199)):b.weapon+1322;
    if(!Readable(ammo,2)||REX_LOAD_U16(ammo)!=0)continue;
    if(original.r1.u32<8192||!Readable(original.r1.u32-8192,8192))continue;
    // Reject pickups through scenery. Stop just short of the embedded blade.
    auto call=original;call.r1.u32-=4096;
    const auto from=call.r1.u32+256,to=from+16,out=to+16;
    for(unsigned i=0;i<3;++i){
      const float start=value(b.player+48+4*i)+(i==1?65.f:0.f);
      const float end=value(b.object+48+4*i);
      REX_STORE_U32(from+4*i,std::bit_cast<uint32_t>(start));
      REX_STORE_U32(to+4*i,std::bit_cast<uint32_t>(start+(end-start)*0.9f));
    }
    REX_STORE_U32(out,0);
    for(auto offset:{84u,92u,100u})REX_STORE_U32(call.r1.u32+offset,0);
    call.r3.u64=b.player;call.r4.u64=from;call.r5.u64=to;call.r6.u64=4;
    call.r7.u64=out;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;
    sub_82240620(call,base);
    if(call.r3.u8)continue;
    const auto destroy=REX_LOAD_U32(b.table);
    if(destroy<0x82100000||destroy>=0x82D00000)continue;
    // Native secondary ammo setter; the primary magazine is never touched.
    call.r3.u64=b.weapon;call.r4.u64=1;sub_82583F38(call,base);
    if(REX_LOAD_U16(ammo)!=1)continue;
    REX_STORE_U8(action+1300,1); // Native KSI attached-bayonet flag.
    blade={}; // Clear before destruction can re-enter the lifecycle hook.
    call.r3.u64=b.object;call.r4.u64=1;
    {auto ctx=call;REX_CALL_INDIRECT_FUNC(destroy);}
    REXLOG_INFO("KSI bayonet: recovered blade {:08X} weapon {:08X}",b.object,b.weapon);
  }
}
}

REX_EXTERN(__imp__sub_82243DA8);
REX_HOOK_RAW(sub_82243DA8) {
  const auto action=ctx.r3.u32;
  const auto previous_weapon=pdz_bayonet::firing_weapon,previous_player=pdz_bayonet::firing_player;
  pdz_bayonet::firing_weapon=pdz_bayonet::firing_player=0;
  if(pdz_input::Supported()&&Readable(action,1308)&&REX_LOAD_U32(action)==0x82097498&&
     REX_LOAD_U32(action+4)==4&&REX_LOAD_U8(action+1300)&&LocalWeaponAction(base,action)){
    const auto weapon=REX_LOAD_U32(action+28),definition=REX_LOAD_U32(weapon+860);
    if(Readable(definition,1148)&&REX_LOAD_U32(definition+1144)==1){
      pdz_bayonet::firing_weapon=weapon;pdz_bayonet::firing_player=REX_LOAD_U32(weapon+92);
    }
  }
  __imp__sub_82243DA8(ctx,base);
  pdz_bayonet::firing_weapon=previous_weapon;pdz_bayonet::firing_player=previous_player;
}
REX_EXTERN(__imp__sub_825CC9C8);
REX_HOOK_RAW(sub_825CC9C8) {
  __imp__sub_825CC9C8(ctx,base);pdz_bayonet::Created(base,ctx.r3.u32);
}
REX_EXTERN(__imp__sub_825982E0);
REX_HOOK_RAW(sub_825982E0) {
  __imp__sub_825982E0(ctx,base);pdz_bayonet::Created(base,ctx.r3.u32);
}
REX_EXTERN(__imp__sub_82515EE0);
REX_HOOK_RAW(sub_82515EE0) {
  pdz_bayonet::Forget(ctx.r3.u32);__imp__sub_82515EE0(ctx,base);
}
