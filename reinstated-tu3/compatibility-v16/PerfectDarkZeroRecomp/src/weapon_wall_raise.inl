// Native obstacle-driven raise. All pointers are scoped to the current camera
// owner; native safe-area flags and firing/reload logic remain authoritative.
namespace {
std::atomic<uint64_t> wallReloadRequest{0};
struct WallRaiseState { uint32_t action=0; float probe=0,fireGrace=0; bool blocked=false,owned=false; uint8_t nativeRaise=0; uint64_t reloadSeen=0; };
thread_local std::array<WallRaiseState,2> wallRaise;
float WallReach(unsigned id){
 switch(id){case 2:case 7:case 11:case 15:case 16:return 52.f;
 case 3:case 5:case 17:case 20:return 70.f;
 case 1:case 8:case 9:case 10:case 12:case 13:case 18:case 23:case 37:case 38:return 95.f;
 default:return 0.f;}
}
bool WallProbe(PPCContext call,uint8_t* base,uint32_t actor,uint32_t view,float reach,float side){
 if(call.r1.u32<8192||!Readable(call.r1.u32-8192,8192))return false;
 call.r1.u32-=4096;
 const auto from=call.r1.u32+256,to=from+16,out=from+32;
 float origin[3],forward[3],right[3];
 for(unsigned i=0;i<3;++i){
  origin[i]=std::bit_cast<float>(REX_LOAD_U32(view+224+i*4));
  forward[i]=-std::bit_cast<float>(REX_LOAD_U32(view+208+i*4));
  right[i]=std::bit_cast<float>(REX_LOAD_U32(view+176+i*4));
  if(!std::isfinite(origin[i])||!std::isfinite(forward[i])||!std::isfinite(right[i]))return false;
 }
 for(float width: {-4.f,0.f,4.f}){
  for(unsigned i=0;i<3;++i){
   const float start=origin[i]+right[i]*(side+width)-(i==1?8.f:0.f);
   REX_STORE_U32(from+i*4,std::bit_cast<uint32_t>(start));
   REX_STORE_U32(to+i*4,std::bit_cast<uint32_t>(start+forward[i]*reach));
  }
  REX_STORE_U32(out,0);
  REX_STORE_U32(call.r1.u32+84,0);REX_STORE_U32(call.r1.u32+92,0);REX_STORE_U32(call.r1.u32+100,0);
  call.r3.u64=actor;call.r4.u64=from;call.r5.u64=to;call.r6.u64=4;
  call.r7.u64=out;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;
  sub_82239358(call,base);
  if(call.r3.u8)return true;
 }
 return false;
}
bool UpdateWallRaise(PPCContext ctx,uint8_t* base,uint32_t action){
 if(!pdz_input::Supported()||!Readable(action,64))return false;
 const auto weapon=REX_LOAD_U32(action+28);
 if(!Readable(weapon,1320)||REX_LOAD_U32(weapon+1280)!=action)return false;
 const auto actor=REX_LOAD_U32(weapon+92),camera=REX_LOAD_U32(0x82D68320),session=REX_LOAD_U32(0x82D6B2B0);
 if(!Readable(actor,0x16A4)||!Readable(camera,0x540)||REX_LOAD_U32(camera+8)!=actor)return false;
 const auto mode=REX_LOAD_U32(camera+0x53C),view=REX_LOAD_U32(camera+60);
 if(!Readable(mode,12)||REX_LOAD_U8(mode+11)>1||!Readable(view,240)||!Readable(session,40)||
    REX_LOAD_U8(session)||REX_LOAD_U8(actor+0x16A3)||pdz_surface::IntroCameraActive())return false;
 const auto first=REX_LOAD_U32(actor+4076),second=REX_LOAD_U32(actor+4080);
 if(weapon!=first&&weapon!=second)return false;
 auto& s=wallRaise[weapon==first?0:1];
 const auto reloadRequest=wallReloadRequest.load();
 if(s.action!=action){s={action};s.reloadSeen=reloadRequest;}
 if(s.reloadSeen!=reloadRequest){
  s.reloadSeen=reloadRequest;
  const auto currentState=REX_LOAD_U32(action+4);
  if(uint32_t(reloadRequest>>32)==actor&&s.owned&&
     (currentState==15||currentState==16||currentState==17)){
   // Input may already have been rejected by the native raised-state gate.
   // Replay that single request through normal reload after releasing our pose.
   s.owned=false;s.fireGrace=.25f;
   REX_STORE_U8(action+47,s.nativeRaise);
   REX_STORE_U32(action+4,1);REX_STORE_U32(action+8,1);
   auto call=ctx;call.r3.u64=weapon;sub_825774F8(call,base);
   return false;
  }
 }
 const auto def=REX_LOAD_U32(weapon+860);
 const float reach=Readable(def,1148)?WallReach(REX_LOAD_U32(def+1144)):0.f;
 const float dt=std::isfinite(ctx.f1.f64)?std::clamp(float(ctx.f1.f64),0.f,.1f):0.f;
 const bool firing=REX_LOAD_U8(action+45)!=0;
 s.fireGrace=firing?.25f:std::max(0.f,s.fireGrace-dt);
 s.probe-=dt;
 const auto state=REX_LOAD_U32(action+4);
 if(s.owned&&state!=15&&state!=16&&state!=17){REX_STORE_U8(action+47,s.nativeRaise);s.owned=false;}
 if(reach>0&&s.fireGrace==0&&s.probe<=0){
  s.probe=1.f/30.f;s.blocked=WallProbe(ctx,base,actor,view,reach+(s.blocked?8.f:0.f),weapon==first?10.f:-10.f);
 }
 const bool wanted=reach>0&&s.blocked&&s.fireGrace==0;
 if(s.owned&&!wanted){
  s.owned=false;
  REX_STORE_U8(action+47,s.nativeRaise);
  if(firing){
   // Enter the native idle/fire dispatcher immediately, without waiting for
   // the lowering animation. Do not alter ammo, shot cadence or safe-area flags.
   REX_STORE_U32(action+4,1);REX_STORE_U32(action+8,1);
   auto call=ctx;call.r3.u64=action;sub_82233DF8(call,base);
  }
  else if(state==16){
   // Animation completion changes raising (15) to the persistent hold (16).
   // The held pose has no further completion event, so dispatch its native
   // release explicitly when clearance returns. This starts lowering (17/26).
   auto call=ctx;call.r3.u64=action;sub_82233DF8(call,base);
  }
 }
 if(wanted&&!s.owned&&state==1&&!REX_LOAD_U8(action+47)&&!REX_LOAD_U8(action+48)){
  s.nativeRaise=REX_LOAD_U8(action+47);
  REX_STORE_U32(action+8,15);REX_STORE_U8(action+47,1);
  auto call=ctx;call.r3.u64=action;sub_82233DF8(call,base);
  s.owned=REX_LOAD_U32(action+4)==15;
  if(!s.owned)REX_STORE_U8(action+47,s.nativeRaise);
 }
 return wanted&&s.owned;
}
}

void pdz_weapons::PollWallReload(uint32_t player,bool held){
 // Called under the input poll mutex; publish only button-down edges.
 static bool wasHeld=false;
 static uint32_t sequence=0;
 if(held&&!wasHeld&&player)wallReloadRequest.store((uint64_t(player)<<32)|++sequence);
 wasHeld=held;
}

// Native setters run outside the update as well. Maintain the obstacle hold
// through animation completion while remembering the genuine native request.
void WallRaiseRequest(PPCContext& ctx){
 for(auto& s:wallRaise)if(s.action==ctx.r3.u32&&s.owned){
  s.nativeRaise=ctx.r4.u8;
  if(s.blocked&&s.fireGrace==0)ctx.r4.u64=1;
  break;
 }
}
REX_EXTERN(__imp__sub_82235998);
REX_HOOK_RAW(sub_82235998){WallRaiseRequest(ctx);__imp__sub_82235998(ctx,base);}
REX_EXTERN(__imp__sub_822414A0);
REX_HOOK_RAW(sub_822414A0){WallRaiseRequest(ctx);__imp__sub_822414A0(ctx,base);}

// Release only our obstacle pose before the ordinary animated reload request.
// Native reload eligibility, reserve consumption and animation remain intact.
REX_EXTERN(__imp__sub_825774F8);
REX_HOOK_RAW(sub_825774F8){
 if(pdz_input::Supported()&&Readable(ctx.r3.u32,1284)){
  const auto action=REX_LOAD_U32(ctx.r3.u32+1280);
  for(auto& s:wallRaise)if(s.action==action&&s.owned&&Readable(action,64)){
   const auto state=REX_LOAD_U32(action+4);
   if(state==15||state==16||state==17){
    s.owned=false;s.fireGrace=.25f;
    REX_STORE_U8(action+47,s.nativeRaise);
    REX_STORE_U32(action+4,1);REX_STORE_U32(action+8,1);
   }
   break;
  }
 }
 __imp__sub_825774F8(ctx,base);
}


