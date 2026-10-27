// Captured at the user's Nightclub stairway entrance.
constexpr Point door_anchor{2412.44824f,1659.45691f,-2781.80591f};
std::array<uint32_t,3> door_guards{};
std::array<bool,3> door_assigned{};
std::array<Point,3> door_posts{};
auto door_next=std::chrono::steady_clock::time_point{};
bool DoorMember(uint32_t a){return a&&std::find(door_guards.begin(),door_guards.end(),a)!=door_guards.end();}
// The native setter writes the physics position, not actor+48. Preserve the
// offset between those two coordinate origins when requesting an actor position.
bool DoorMove(PPCContext call,uint8_t* base,uint32_t actor,Point target){
 const auto component=REX_LOAD_U32(actor+228);if(!Readable(component,4))return false;
 const auto table=REX_LOAD_U32(component);if(!Readable(table,20))return false;
 const auto fn=REX_LOAD_U32(table+16);if(fn<0x82100000||fn>=0x82D00000)return false;
 call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return false;
 call.r3.u64=component;{auto ctx=call;REX_CALL_INDIRECT_FUNC(fn);call=ctx;}
 if(!Readable(call.r3.u32,52))return false;
 const auto physics=Read(base,call.r3.u32+40),origin=Read(base,actor+48);
 if(!Finite(physics)||!Finite(origin)||!Finite(target))return false;
 target.x+=physics.x-origin.x;target.y+=physics.y-origin.y;target.z+=physics.z-origin.z;
 Write(base,call.r1.u32+256,target);call.r3.u64=actor;call.r4.u64=call.r1.u32+256;sub_82215ED8(call,base);return true;
}
void DoorTick(PPCContext ctx,uint8_t* base){
 if(!SocialMission(base))return;
 const auto session=REX_LOAD_U32(0x82D6BFD8),player=SocialPlayer(base);
 if(REX_LOAD_U8(session)||!Readable(player,4084)||!pdz_input::CrosshairVisible())return;
 const auto now=std::chrono::steady_clock::now();
 // Keep locomotion at the post without freezing AI, turning or melee animations.
 for(unsigned slot=0;slot<door_guards.size();++slot){
  const auto actor=door_guards[slot];
  auto tracked=std::find_if(actors.begin(),actors.end(),[&](const Actor& a){return a.address==actor;});
  if(!actor||tracked==actors.end()||!Eligible(ctx,base,*tracked,player))continue;
  const auto nav=REX_LOAD_U32(actor+12528);
  if(Readable(nav,192))REX_STORE_U32(nav+184,0);
  Point post=door_posts[slot];
  const auto current=Read(base,actor+48);
  // Preserve gravity and stair-floor settling; only restrain horizontal drift.
  if(Finite(current)&&std::hypot(current.x-post.x,current.z-post.z)>2.f){
   post.y=current.y;DoorMove(ctx,base,actor,post);
  }
  const auto position=Read(base,player+48);
  const bool close=Finite(position)&&std::hypot(position.x-post.x,position.z-post.z)<220.f&&std::abs(position.y-post.y)<180.f;
  auto& state=suspicion[actor];
  if(close||state.aiming)GuardAim(ctx,base,actor,player,close);
  state.aiming=close;
 }
 if(now<door_next)return;
 door_next=now+std::chrono::seconds(6);
 for(unsigned slot=0;slot<door_guards.size();++slot)if(!door_assigned[slot]){
  for(auto& a:actors){
   if(!a.clone||LeaveMember(a.address)||DoorMember(a.address)||!SocialGuard(base,a.address)||
      REX_LOAD_U32(a.address+5280)!=48||!Eligible(ctx,base,a,player))continue;
   // Keep the front post; place the second guard across the stairs, not
   // along the old camera-derived axis (which ran up the stairway).
   Point place=door_anchor;place.x-=55.f*.985545f;
   place.z+=55.f*.169414f;
   place.z-=110.f*float(slot);
   const auto origin=Read(base,a.address+48);Point sourceFloor{},floor{};
   if(!Ray(ctx,base,a.address,{origin.x,origin.y+30,origin.z},{origin.x,origin.y-350,origin.z},&sourceFloor)||!Finite(sourceFloor))continue;
   if(!Ray(ctx,base,a.address,{place.x,place.y+100,place.z},{place.x,place.y-600,place.z},&floor)||!Finite(floor))continue;
   const float offset=origin.y-sourceFloor.y;if(offset< -30||offset>350)continue;
   place.y=floor.y+offset+2;
   if(!DoorMove(ctx,base,a.address,place))continue;
   door_posts[slot]=place;
   door_guards[slot]=a.address;door_assigned[slot]=true;suspicion[a.address].exposure={};
   REXLOG_INFO("Door guard copy placed: slot={} actor={:08X}",slot,a.address);break;
  }
 }
 const auto p=Read(base,player+48);
 if(!Finite(p)||std::hypot(p.x-door_anchor.x,p.z-door_anchor.z)>350||std::abs(p.y-door_anchor.y)>220)return;
 for(const auto actor:door_guards){
  if(!Readable(actor,13040))continue;
  auto& state=suspicion[actor];
  if(!Unarmed(base,player)){state.exposure.hostile=true;continue;}
  if(state.exposure.hostile)continue;
  GuardFace(ctx,base,actor,player);
  GuardVoice(ctx,base,actor,"XACT_SOUNDBANK_GANGSTA2_PLAYER_CLUB_FRONT_WARN");
  LeaveSay("Security: You can't pass. This entrance is off limits.");break;
 }
}
