// Included in coop_ally.cpp's private namespace. All native calls run on the
// guest simulation thread; input/render threads see only atomic status values.
using SyncPoint = pdz_coop::Position;
enum class SyncMovement { None, Follow, Position, Face };
SyncMovement sync_movement=SyncMovement::None;
bool sync_old_suppressed=false;
SyncPoint sync_face_destination{};

float SyncDistance(SyncPoint a,SyncPoint b) {
  return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
SyncPoint ReadPoint(uint8_t* base,uint32_t address,float scale=1.f) {
  return {std::bit_cast<float>(REX_LOAD_U32(address))*scale,
          std::bit_cast<float>(REX_LOAD_U32(address+4))*scale,
          std::bit_cast<float>(REX_LOAD_U32(address+8))*scale};
}
void WritePoint(uint8_t* base,uint32_t address,SyncPoint p,float scale=100.f) {
  REX_STORE_U32(address,std::bit_cast<uint32_t>(p.x*scale));
  REX_STORE_U32(address+4,std::bit_cast<uint32_t>(p.y*scale));
  REX_STORE_U32(address+8,std::bit_cast<uint32_t>(p.z*scale));
}
unsigned WeaponId(uint8_t* base,uint32_t weapon) {
  if(!Readable(weapon,1340)||REX_LOAD_U32(weapon+12)!=4)return 0;
  const auto definition=REX_LOAD_U32(weapon+860);
  return Readable(definition,1148)?REX_LOAD_U32(definition+1144):0;
}
uint32_t HeldWeapon(uint8_t* base,uint32_t actor) {
  if(!Readable(actor,4084))return 0;
  const auto second=REX_LOAD_U32(actor+4080),first=REX_LOAD_U32(actor+4076);
  return WeaponId(base,second)?second:(WeaponId(base,first)?first:0);
}
bool Suppressed(uint8_t* base,uint32_t weapon) {
  if(!WeaponId(base,weapon))return false;
  const auto fire=REX_LOAD_U32(weapon+1280);
  return Readable(fire,84)?(REX_LOAD_U8(fire+83)&REX_LOAD_U8(fire+65))!=0:
                         (REX_LOAD_U32(weapon+1232)&1)!=0;
}
uint32_t FindWeapon(PPCContext call,uint8_t* base,unsigned id) {
  if(!id||!Readable(ally,5216))return 0;
  call.r1.u32-=2048;
  const auto out=call.r1.u32+256;
  if(!Readable(out,4))return 0;
  REX_STORE_U32(out,0);
  call.r3.u64=ally+5164;call.r4.u64=id;call.r5.u64=out;call.r6.u64=0;
  sub_824F6478(call,base);
  const auto weapon=REX_LOAD_U32(out);
  return WeaponId(base,weapon)==id&&REX_LOAD_U32(weapon+92)==ally?weapon:0;
}
bool LoadedWeapon(uint8_t* base,unsigned id) {
  const auto registry=REX_LOAD_U32(0x82D6BFD8+48);
  if(!Readable(registry,25))return false;
  const auto records=REX_LOAD_U32(registry);
  const auto count=REX_LOAD_U8(registry+24);
  if(!Readable(records,count*2196u))return false;
  for(unsigned i=0;i<count;++i) {
    const auto entry=records+i*2196u,def=REX_LOAD_U32(entry);
    if(Readable(def,1148)&&REX_LOAD_U32(def+1144)==id)
      return Readable(REX_LOAD_U32(entry+8),4);
  }
  return false;
}
void EquipSyncWeapon(PPCContext call,uint8_t* base,uint32_t weapon) {
  call.r1.u32-=2048;call.r3.u64=ally;call.r4.u64=weapon;call.r5.u64=0;
  sub_8254DCD8(call,base);
}
bool SetSuppressed(PPCContext call,uint8_t* base,uint32_t weapon,bool suppressed) {
  if(!WeaponId(base,weapon))return false;
  const auto fire=REX_LOAD_U32(weapon+1280);
  if(!Readable(fire,84))return false;
  if(Suppressed(base,weapon)==suppressed)return true;
  const auto options=REX_LOAD_U32(fire+72);
  if(!Readable(options,8))return false;
  const auto count=REX_LOAD_U8(options);
  const auto entries=REX_LOAD_U32(options+4);
  if(!count||count>16||!Readable(entries,count*36))return false;
  for(unsigned i=0;i<count;++i)if(REX_LOAD_U32(entries+i*36+28)==3) {
    call.r1.u32-=2048;call.r3.u64=fire;call.r4.u64=i;call.r5.u64=1;
    sub_8223DA50(call,base);
    return Suppressed(base,weapon)==suppressed;
  }
  return false;
}
bool HeadPoint(PPCContext call,uint8_t* base,uint32_t actor,SyncPoint& head) {
  if(!Readable(actor,76))return false;
  const auto model=REX_LOAD_U32(actor+72);
  if(!Readable(model,80))return false;
  const auto def=REX_LOAD_U32(model);
  if(!Readable(def,16))return false;
  const auto skeleton=REX_LOAD_U32(def+12);
  if(!Readable(skeleton,56))return false;
  const auto count=REX_LOAD_U32(skeleton+48),frame=REX_LOAD_U32(0x82FBE3C0);
  if(count<=17||count>256||frame>3||
     !REX_LOAD_U32(model+60)||
     !Readable(REX_LOAD_U32(skeleton+52),count*36)||
     !Readable(REX_LOAD_U32(model+60+frame*4),count*64)||
     !Readable(REX_LOAD_U32(model+40+frame*4),64))return false;
  call.r1.u32-=2048;
  if(!Readable(call.r1.u32,2048))return false;
  call.r3.u64=model;call.r4.u64=17;call.r5.u64=call.r1.u32+256;call.r6.u64=call.r1.u32+320;
  sub_82413B18(call,base);
  head=ReadPoint(base,call.r1.u32+368,.01f);
  return pdz_coop::SyncFinite(head)&&SyncDistance(head,ActorPosition(base,actor))<3.f;
}
bool ClearHeadRay(PPCContext call,uint8_t* base,uint32_t shooter,uint32_t target,
                  SyncPoint origin,SyncPoint head) {
  if(!pdz_coop::SyncFinite(origin)||!pdz_coop::SyncFinite(head))return false;
  call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return false;
  const auto from=call.r1.u32+256,to=call.r1.u32+272,out=call.r1.u32+288;
  WritePoint(base,from,origin);WritePoint(base,to,head);REX_STORE_U32(out,0);
  REX_STORE_U32(call.r1.u32+84,0);REX_STORE_U32(call.r1.u32+92,0);REX_STORE_U32(call.r1.u32+100,0);
  call.r3.u64=shooter;call.r4.u64=from;call.r5.u64=to;call.r6.u64=4;
  call.r7.u64=out;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;
  // Native crosshair query: world + dynamic geometry, exact segment, no damage.
  sub_82240620(call,base);
  return call.r3.u8&&REX_LOAD_U32(out)==target;
}
bool SyncView(uint8_t* base,SyncPoint& origin,SyncPoint& forward,SyncPoint& right) {
  if(!pdz_input::CrosshairVisible())return false;
  const auto camera=REX_LOAD_U32(0x82D69048);
  if(!Readable(camera,0x540)||REX_LOAD_U32(camera+8)!=player)return false;
  const auto mode=REX_LOAD_U32(camera+0x53C),view=REX_LOAD_U32(camera+60);
  if(!Readable(mode,12)||REX_LOAD_U8(mode+11)>1||!Readable(view,240))return false;
  origin=ReadPoint(base,view+224,.01f);forward=ReadPoint(base,view+208,-1.f);right=ReadPoint(base,view+176);
  return pdz_coop::SyncFinite(origin)&&pdz_coop::SyncFinite(forward)&&pdz_coop::SyncFinite(right);
}
void SetSyncAim(PPCContext call,uint8_t* base,const SyncPoint* head) {
  if(!Readable(ally,6232))return;
  call.r1.u32-=2048;
  if(head)WritePoint(base,call.r1.u32+256,*head);
  call.r3.u64=ally;call.r4.u64=head?call.r1.u32+256:0;
  sub_8252CF58(call,base);
}
void EndSync(PPCContext ctx,uint8_t* base,bool restore=true) {
  sync_policy.Reset();sync_status=0;sync_sample={};sync_player_weapon=0;
  if(sync_formation&&Readable(ally,13040)) {
    StopFollowing(ctx,base);SetSyncAim(ctx,base,nullptr);
    if(restore) {
      if(WeaponId(base,sync_weapon)&&REX_LOAD_U32(sync_weapon+92)==ally)
        SetSuppressed(ctx,base,sync_weapon,sync_old_suppressed);
      if(const auto previous=FindWeapon(ctx,base,sync_saved_weapon))EquipSyncWeapon(ctx,base,previous);
    }
  }
  sync_formation=false;sync_movement=SyncMovement::None;
  sync_saved_weapon=0;sync_weapon=0;
}
bool PrepareSyncWeapon(PPCContext call,uint8_t* base) {
  if(!sync_weapon) {
    for(const auto id:{2u,3u,9u})if((sync_weapon=FindWeapon(call,base,id)))break;
    if(!sync_weapon)for(const auto id:{2u,3u,9u})if(LoadedWeapon(base,id)) {
      auto give=call;give.r1.u32-=2048;give.r3.u64=ally;give.r4.u64=id;
      give.r5.u64=0;give.r6.u64=0;give.r7.u64=0;give.r8.u64=0;
      sub_8254C378(give,base);sync_weapon=FindWeapon(call,base,id);
      if(sync_weapon)break;
    }
    if(!sync_weapon)return false;
    sync_old_suppressed=Suppressed(base,sync_weapon);
    EquipSyncWeapon(call,base,sync_weapon);
  }
  if(HeldWeapon(base,ally)!=sync_weapon)return false;
  if(!SetSuppressed(call,base,sync_weapon,Suppressed(base,HeldWeapon(base,player))))return false;
  const auto fire=REX_LOAD_U32(sync_weapon+1280);
  if(!REX_LOAD_U16(sync_weapon+1316)&&Readable(fire,8)&&
     (REX_LOAD_U32(fire+4)==1||REX_LOAD_U32(fire+4)==12)) {
    auto reload=call;reload.r1.u32-=2048;reload.r3.u64=sync_weapon;
    sub_82586D80(reload,base); // Ordinary animated reload spends reserve ammo.
    return false;
  }
  return true;
}
void QueueSyncPoint(PPCContext call,uint8_t* base,SyncPoint point,bool face) {
  StopFollowing(call,base);
  call.r1.u32-=2048;WritePoint(base,call.r1.u32+256,point);
  call.r3.u64=face?80:72;sub_821784F0(call,base);
  const auto task=call.r3.u32;if(!task)return;
  call.r4.u64=call.r1.u32+256;call.r5.u64=1;call.r6.u64=0;call.r7.u64=0;
  if(face)sub_821A14A0(call,base);else sub_821A4C30(call,base);
  call.r3.u64=follow_reference;call.r4.u64=task;sub_821433D8(call,base);
  call.r3.u64=ally+7912;call.r4.u64=task;sub_8217F0F0(call,base);
  sync_movement=face?SyncMovement::Face:SyncMovement::Position;
  if(face)sync_face_destination=point;
}
bool FacingSyncHead(PPCContext call,uint8_t* base,SyncPoint head) {
  call.r1.u32-=2048;WritePoint(base,call.r1.u32+256,head);
  call.r3.u64=ally;call.r4.u64=call.r1.u32+256;
  sub_82519B90(call,base);
  return std::isfinite(call.f1.f64)&&std::abs(call.f1.f64)<.10;
}
bool NativeShotReady(PPCContext call,uint8_t* base) {
  if(!WeaponId(base,sync_weapon)||HeldWeapon(base,ally)!=sync_weapon||REX_LOAD_U32(sync_weapon+92)!=ally)return false;
  const auto fire=REX_LOAD_U32(sync_weapon+1280);
  const auto effects=REX_LOAD_U32(sync_weapon+1096);
  if(!Readable(fire,84)||(REX_LOAD_U32(fire+4)!=1&&REX_LOAD_U32(fire+4)!=12)||
     REX_LOAD_U32(fire+28)!=sync_weapon||!Readable(effects,16)||
     REX_LOAD_U32(effects+12)!=sync_weapon||!REX_LOAD_U16(sync_weapon+1316))return false;
  const auto table=REX_LOAD_U32(fire);
  if(!Readable(table,24))return false;
  const auto fn=REX_LOAD_U32(table+20);
  if(fn<0x82100000||fn>=0x82D00000)return false;
  call.r1.u32-=2048;call.r3.u64=fire;
  auto ctx=call;REX_CALL_INDIRECT_FUNC(fn);
  return ctx.r3.u8!=0;
}
bool SyncMuzzle(PPCContext call,uint8_t* base,SyncPoint& muzzle) {
  if(!WeaponId(base,sync_weapon))return false;
  call.r1.u32-=2048;const auto out=call.r1.u32+256;
  call.r3.u64=sync_weapon;call.r4.u64=out;call.r5.u64=0;
  sub_82585B58(call,base);muzzle=ReadPoint(base,out,.01f);
  return pdz_coop::SyncFinite(muzzle)&&SyncDistance(muzzle,ActorPosition(base,ally))<3.f;
}
pdz_coop::SyncDecision UpdateSync(PPCContext ctx,uint8_t* base,bool confirmedShot=false) {
  using namespace pdz_coop;
  SyncPoint origin,forward,right;
  if(!Campaign(base)||!Readable(player,13040)||!Readable(ally,13040)||
     rescue.IsDown(Partner::Player)||rescue.IsDown(Partner::Ally)||
     REX_LOAD_U8(REX_LOAD_U32(0x82D6BFD8))||REX_LOAD_U8(player+0x16A3)||
     pdz_surface::IntroCameraActive()||pdz_surface::EndingCameraActive()||
     !SyncView(base,origin,forward,right)||Health(ctx,base,player)<=0||Health(ctx,base,ally)<=0) {
    EndSync(ctx,base);return {};
  }
  const auto weapon=HeldWeapon(base,player);
  if(!weapon){EndSync(ctx,base);return {};}
  const auto now=std::chrono::steady_clock::now();
  const double dt=sync_sample.time_since_epoch().count()?std::chrono::duration<double>(now-sync_sample).count():0;
  if(!confirmedShot&&dt>0&&dt<.025)return {sync_formation,sync_policy.Target(),{},false};
  if(sync_player_weapon&&sync_player_weapon!=weapon)EndSync(ctx,base);
  sync_player_weapon=weapon;sync_sample=now;
  std::vector<SyncCandidate> nearby;
  Actor focus=0;float closest=80.f;
  for(const auto& c:characters) {
    if(c.address==ally||c.address==player||!Readable(c.address,13040)||
       SyncDistance(ActorPosition(base,c.address),origin)>80.f||Health(ctx,base,c.address)<=0||
       !HostileToPlayer(ctx,base,c.address))continue;
    SyncPoint head;if(!HeadPoint(ctx,base,c.address,head))continue;
    const auto handle=Handle(c.address);nearby.push_back({handle,head,true,true,true});
    const auto distance=SyncDistance(origin,head);
    if(distance<closest&&HeadRayFocus(origin,forward,head,80.f,.16f)&&
       ClearHeadRay(ctx,base,player,c.address,origin,head)){focus=handle;closest=distance;}
  }
  std::vector<SyncCandidate> group;
  if(focus) {
    const auto focused=std::find_if(nearby.begin(),nearby.end(),[&](const auto& c){return c.actor==focus;});
    for(const auto& c:nearby)if(SyncDistance(c.head,focused->head)<=10.f&&
       ClearHeadRay(ctx,base,player,uint32_t(c.actor),origin,c.head))group.push_back(c);
  }
  SyncInput input;input.player=Handle(player);input.ally=Handle(ally);input.headFocus=focus;
  input.group=group.data();input.groupCount=unsigned(group.size());input.elapsedSeconds=dt;
  input.enabled=input.playerAlive=input.allyAlive=true;
  auto decision=sync_policy.Tick(input);
  if(!decision.active){if(sync_formation)EndSync(ctx,base);return {};}
  if(!sync_formation) {
    sync_formation=true;sync_saved_weapon=WeaponId(base,HeldWeapon(base,ally));
    StopFollowing(ctx,base);SetScript(ctx,base,0);last_order=Order::Hold;
    REXLOG_INFO("Bot co-op: coordinated headshot locked, player target {:08X}, ally target {:08X}",uint32_t(focus),uint32_t(decision.target));
  }
  sync_status=1;
  SetSyncAim(ctx,base,&decision.targetHead);
  const bool weaponReady=PrepareSyncWeapon(ctx,base);
  const float forwardLength=std::hypot(forward.x,forward.z),rightLength=std::hypot(right.x,right.z);
  if(forwardLength<.1f||rightLength<.1f)return decision;
  const auto playerPos=ActorPosition(base,player),allyPos=ActorPosition(base,ally);
  const SyncPoint destination{playerPos.x+.9f*right.x/rightLength+.65f*forward.x/forwardLength,
                              playerPos.y,playerPos.z+.9f*right.z/rightLength+.65f*forward.z/forwardLength};
  bool inPosition=SyncDistance(destination,allyPos)<.65f;
  const bool facing=FacingSyncHead(ctx,base,decision.targetHead);
  if(!confirmedShot) {
    if(!Near(base,3.f)||!Sees(ctx,base,ally,player)) {
      if(sync_movement!=SyncMovement::Follow){StopFollowing(ctx,base);sync_movement=SyncMovement::Follow;}
      Follow(ctx,base);
    } else if(!inPosition) {
      const auto task=Readable(follow_reference,8)?REX_LOAD_U32(follow_reference+4):0;
      if(sync_movement!=SyncMovement::Position||SyncDistance(sync_destination,destination)>.45f||
         !Readable(task,36)||REX_LOAD_U32(task+12)>=2) {
        QueueSyncPoint(ctx,base,destination,false);sync_destination=destination;
      }
    } else {
      const auto task=Readable(follow_reference,8)?REX_LOAD_U32(follow_reference+4):0;
      if(sync_movement!=SyncMovement::Face||(!facing&&
          (!Readable(task,36)||REX_LOAD_U32(task+12)>=2)))
        QueueSyncPoint(ctx,base,decision.targetHead,true);
      else if(Readable(task,80)&&REX_LOAD_U32(task+12)<2&&SyncDistance(sync_face_destination,decision.targetHead)>.1f) {
        auto turn=ctx;turn.r1.u32-=2048;WritePoint(base,turn.r1.u32+256,decision.targetHead);
        turn.r3.u64=task;turn.r4.u64=turn.r1.u32+256;sub_821A16E8(turn,base);
        sync_face_destination=decision.targetHead;
      }
    }
  }
  SyncPoint muzzle;
  const bool ready=inPosition&&facing&&weaponReady&&NativeShotReady(ctx,base)&&
                   SyncMuzzle(ctx,base,muzzle)&&ClearHeadRay(ctx,base,ally,uint32_t(decision.target),muzzle,decision.targetHead);
  sync_status=ready?2:1;
  input.elapsedSeconds=0;input.confirmedPlayerShot=confirmedShot;input.botReady=ready;input.botAimTarget=decision.target;
  return sync_policy.Tick(input);
}

SyncPoint sync_shot_head{},sync_shot_origin{};
pdz_coop::Actor sync_shot_target=0;
bool sync_discharged=false;
void FireSynchronized(PPCContext call,uint8_t* base,const pdz_coop::SyncDecision& plan) {
  const auto target=uint32_t(plan.target);
  if(!plan.fire||!Campaign(base)||!Readable(player,13040)||!Readable(ally,13040)||
     rescue.IsDown(pdz_coop::Partner::Player)||rescue.IsDown(pdz_coop::Partner::Ally)||
     Health(call,base,player)<=0||Health(call,base,ally)<=0||
     Handle(target)!=plan.target||!Readable(target,13040)||
     Health(call,base,target)<=0||!NativeShotReady(call,base)||
     !SyncMuzzle(call,base,sync_shot_origin)||
     !ClearHeadRay(call,base,ally,target,sync_shot_origin,plan.targetHead))return;
  sync_shot_head=plan.targetHead;sync_shot_target=plan.target;
  sync_discharged=false;sync_firing=true;
  const auto fire=REX_LOAD_U32(sync_weapon+1280);
  call.r1.u32-=2048;const auto event=call.r1.u32+256;
  std::memset(base+event,0,12);REX_STORE_U16(event+4,1);
  call.r3.u64=fire;call.r4.u64=event;
  // The ordinary firing event spends a round and emits native muzzle flash,
  // sound/recoil and a collision-tested bullet, without queuing a later shot.
  sub_8223AC88(call,base);
  sync_firing=false;
  REXLOG_INFO("Bot co-op: synchronized shot target {:08X}, weapon {}, suppressed={}, discharged={}",
              target,WeaponId(base,sync_weapon),Suppressed(base,sync_weapon),sync_discharged);
}
