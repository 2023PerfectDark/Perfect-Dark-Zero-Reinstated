// Nightclub parking encounter, anchored to the user's captured location.
constexpr Point leave_anchor{433.97723f,201.48683f,1693.35303f};
// Captured from the live security group next to the parked cars.
constexpr Point leave_parking{-850.f,211.f,850.f};
uint32_t leave_guard=0;
std::array<uint32_t,5> leave_backup{};
std::array<uint32_t,5> leave_tasks{};
pdz_leave::Sequence leave_sequence;
auto leave_last=std::chrono::steady_clock::time_point{};
std::mutex leave_message_mutex;
std::string leave_message;
auto leave_message_until=std::chrono::steady_clock::time_point{};
bool LeaveMember(uint32_t a){return a&&(a==leave_guard||std::find(leave_backup.begin(),leave_backup.end(),a)!=leave_backup.end());}
void LeaveSay(const char* text){std::lock_guard lock(leave_message_mutex);leave_message=text;leave_message_until=std::chrono::steady_clock::now()+std::chrono::seconds(5);}
void LeaveReset(){leave_guard=0;leave_backup.fill(0);leave_tasks.fill(0);leave_sequence={};leave_last={};LeaveSay("");}
void LeaveApproach(PPCContext call,uint8_t* base,unsigned slot,uint32_t player){
 const auto actor=leave_backup[slot];if(!Readable(actor,13040))return;
 const auto ref=leave_tasks[slot];
 if(!Readable(ref,8))return;
 const auto old=REX_LOAD_U32(ref+4);
 if(Readable(old,36)&&REX_LOAD_U32(old+12)<2)return;
 const auto pad=REX_LOAD_U32(player+180);if(!Readable(pad,66))return;
 call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
 const auto nav=REX_LOAD_U32(actor+12528);if(!Readable(nav,192))return;
 // Native walking animation speed, rather than teleporting the backup.
 call.r3.u64=actor;call.r4.u64=107;call.r5.u64=0;sub_821F0080(call,base);
 const auto anim=call.r3.u32;
 if(Readable(anim,24)&&Readable(REX_LOAD_U32(anim+20),12)){
  sub_821F1C58(call,base);const float speed=float(call.f1.f64);
  if(std::isfinite(speed)&&speed>0)REX_STORE_U32(nav+184,std::bit_cast<uint32_t>(speed));
 }
 call.r3.u64=nav;call.r4.u64=1;sub_8218FBF0(call,base);
 call.r3.u64=96;sub_821784F0(call,base);const auto task=call.r3.u32;if(!task)return;
 call.r4.u64=pad;call.r5.u64=0;call.r6.u64=1;call.r7.u64=0;call.r8.u64=0;sub_821A3438(call,base);
 call.r3.u64=ref;call.r4.u64=task;sub_821433D8(call,base);
 call.r3.u64=actor+7912;call.r4.u64=task;sub_8217F0F0(call,base);
}
void LeaveStop(PPCContext ctx,uint8_t* base){
 for(unsigned i=0;i<5;++i){
  const auto actor=leave_backup[i],ref=leave_tasks[i];if(!Readable(actor,13040)||!Readable(ref,8))continue;
  const auto task=REX_LOAD_U32(ref+4);if(!Readable(task,36)||REX_LOAD_U32(task+12)>=2)continue;
  const auto table=REX_LOAD_U32(task);if(!Readable(table,28))continue;
  const auto fn=REX_LOAD_U32(table+24);if(fn<0x82100000||fn>=0x82D00000)continue;
  auto saved=ctx;ctx.r1.u32-=2048;if(!Readable(ctx.r1.u32,2048)){ctx=saved;continue;}
  ctx.r3.u64=task;REX_CALL_INDIRECT_FUNC(fn);
  ctx.r3.u64=actor+7912;ctx.r4.u64=task;sub_821800B0(ctx,base);ctx=saved;
 }
}
void LeaveTick(PPCContext ctx,uint8_t* base){
 if(!SocialMission(base))return;
 const auto session=REX_LOAD_U32(0x82D6BFD8),player=SocialPlayer(base);
 if(REX_LOAD_U8(session)||!Readable(player,4084)||!pdz_input::CrosshairVisible()){leave_last={};return;}
 const auto now=std::chrono::steady_clock::now();
 const double dt=leave_last.time_since_epoch().count()?std::chrono::duration<double>(now-leave_last).count():0.;
 if(dt>0&&dt<.1)return;leave_last=now;
 if(!leave_guard){
  std::vector<std::pair<float,uint32_t>> guards;
  for(auto& a:actors){
   if(DoorMember(a.address)||!a.clone||!SocialGuard(base,a.address)||REX_LOAD_U32(a.address+5280)!=48||!Eligible(ctx,base,a,player))continue;
   const auto p=Read(base,a.address+48);if(!Finite(p)||std::abs(p.y-leave_parking.y)>180)continue;
   const float distance=std::hypot(p.x-leave_parking.x,p.z-leave_parking.z);
   if(distance>600||InsideClub(p))continue;
   guards.emplace_back(distance,a.address);
  }
  if(guards.empty())return;
  std::sort(guards.begin(),guards.end());
  const auto guard=guards.front().second;
  // Reassign the existing parking guard's crowd copy. The original remains
  // at his native parking post and can still join the backup group.
  Point place=leave_anchor;std::vector<Point> occupied{Read(base,player+48)};
  // This exact destination was occupied by the player and verified by a live
  // native-physics relocation. Do not reject it using clone-placement rays.
  if(!Separated(place,occupied))return;
  auto call=ctx;call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
  Write(base,call.r1.u32+256,place);call.r3.u64=guard;call.r4.u64=call.r1.u32+256;sub_82215ED8(call,base);
  leave_guard=guard;
  suspicion[leave_guard].exposure={};
  REXLOG_INFO("Parking warning guard relocated: existing copy {:08X}, body {}, destination ({:.1f},{:.1f},{:.1f}); original stays at parking post",leave_guard,REX_LOAD_U32(leave_guard+5280),place.x,place.y,place.z);
 }
 if(!Readable(leave_guard,13040))return;
 // Backup availability must never prevent the lead guard's relocation.
 // Recruit existing nearby guards, including copies already in the parking
 // group; do not create actors just to satisfy the five-slot backup limit.
 if(!leave_sequence.hostile&&std::find(leave_backup.begin(),leave_backup.end(),0)!=leave_backup.end()){
  std::vector<std::pair<float,uint32_t>> candidates;
  for(const auto& a:actors){
   if(DoorMember(a.address)||LeaveMember(a.address)||!SocialGuard(base,a.address)||!Eligible(ctx,base,a,player))continue;
   const auto body=REX_LOAD_U32(a.address+5280);if(body!=48&&body!=62)continue;
   const auto p=Read(base,a.address+48);
   if(!Finite(p)||std::abs(p.y-leave_parking.y)>400||InsideClub(p))continue;
   const float distance=std::hypot(p.x-leave_parking.x,p.z-leave_parking.z);
   if(distance<=2400)candidates.emplace_back(distance,a.address);
  }
  std::sort(candidates.begin(),candidates.end());unsigned next=0;
  for(unsigned i=0;i<5&&next<candidates.size();++i)if(!leave_backup[i]){
   if(!leave_tasks[i]){leave_tasks[i]=rex::Runtime::instance()->memory()->SystemHeapAlloc(8,8);
    if(leave_tasks[i]){allocations.push_back(leave_tasks[i]);std::memset(base+leave_tasks[i],0,8);}}
   if(!leave_tasks[i])continue;
   leave_backup[i]=candidates[next++].second;suspicion[leave_backup[i]].exposure={};
   REXLOG_INFO("Parking backup slot {} assigned existing guard {:08X}",i+1,leave_backup[i]);
  }
 }
 const auto p=Read(base,player+48);
 const bool inArea=Finite(p)&&std::hypot(p.x-leave_anchor.x,p.z-leave_anchor.z)<650&&std::abs(p.y-leave_anchor.y)<250;
 bool arrived=false;
 if(leave_sequence.called&&inArea&&!leave_sequence.hostile){
  for(unsigned i=0;i<5;++i)if(Readable(leave_backup[i],13040)){
   const auto g=Read(base,leave_backup[i]+48);
   if(std::hypot(g.x-p.x,g.z-p.z)<300&&std::abs(g.y-p.y)<180){arrived=true;}
   else LeaveApproach(ctx,base,i,player);
  }
 }
 if(!inArea&&leave_sequence.called)LeaveStop(ctx,base);
 const auto event=leave_sequence.Tick(dt,inArea,arrived);
 if(event==pdz_leave::Event::Warn){GuardFace(ctx,base,leave_guard,player);GuardVoice(ctx,base,leave_guard,"XACT_SOUNDBANK_GANGSTA2_PLAYER_CLUB_FRONT_WARN");LeaveSay("Guard: You need to leave this area.");}
 if(event==pdz_leave::Event::Backup){LeaveSay("Guard: I asked you three times. Backup, over here!");GuardVoice(ctx,base,leave_guard,"XACT_SOUNDBANK_GANGSTA2_PLAYER_IGNORES_WARNING");}
 if(event==pdz_leave::Event::Final){
  for(auto a:leave_backup)if(Readable(a,13040)){const auto g=Read(base,a+48);if(std::hypot(g.x-p.x,g.z-p.z)<300){GuardFace(ctx,base,a,player);GuardVoice(ctx,base,a,"XACT_SOUNDBANK_GANGSTA2_PLAYER_CLUB_FRONT_WARN");break;}}
  LeaveSay("Backup: Last warning. Leave now.");
 }
 if(event==pdz_leave::Event::Hostile){LeaveStop(ctx,base);LeaveSay("Backup: You were warned!");for(auto a:leave_backup)suspicion[a].exposure.hostile=true;suspicion[leave_guard].exposure.hostile=true;}
}
