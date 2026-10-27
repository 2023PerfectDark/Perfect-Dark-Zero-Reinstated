// Extend the native forward-held climb detector, not the movement/ladder state.
// Its authored-surface probe uses phm15 (0x10000); ordinary scenery uses 2.
namespace pdz_climb {
constexpr float minimum_height=30.f;
thread_local uint32_t probe_owner = 0;
thread_local float probe_floor = 0.f;
struct ProbeScope {
  uint32_t previous;
  float previous_floor;
  ProbeScope(uint32_t owner,float floor):previous(probe_owner),previous_floor(probe_floor){probe_owner=owner;probe_floor=floor;}
  ~ProbeScope(){probe_owner=previous;probe_floor=previous_floor;}
};
}
REX_EXTERN(__imp__sub_825DCCF8);
REX_HOOK_RAW(sub_825DCCF8) {
  // Restrict the broadened mask to this player's fallback detector and its
  // own collision object. Other rays, actors and authored climbs are untouched.
  std::array<uint32_t,2> edited{},saved{};
  unsigned count=0;
  if(pdz_climb::probe_owner && ctx.r3.u32==pdz_climb::probe_owner &&
     (ctx.r4.u32==0x10000 || ctx.r4.u32==0x10002)){
    ctx.r4.u64=2;
    auto lower=[&](uint32_t vector){
      if(!Readable(vector,12))return;
      edited[count]=vector+4;saved[count++]=REX_LOAD_U32(vector+4);
      REX_STORE_U32(vector+4,std::bit_cast<uint32_t>(pdz_climb::probe_floor+pdz_climb::minimum_height));
    };
    // The native forward probe starts at 40 units and the top search stops at
    // 60. Lower these two detection limits only; keep overhead and clearance
    // probes unchanged. Restore the stack vectors before native code reuses them.
    if(ctx.lr==0x824FBFA8){lower(ctx.r6.u32);lower(ctx.r7.u32);}
    else if(ctx.lr==0x824FC124)lower(ctx.r7.u32);
  }
  __imp__sub_825DCCF8(ctx,base);
  for(unsigned i=0;i<count;++i)REX_STORE_U32(edited[i],saved[i]);
}
REX_EXTERN(__imp__sub_824FBE20);
REX_HOOK_RAW(sub_824FBE20) {
  const auto input=ctx;
  __imp__sub_824FBE20(ctx,base);
  if(ctx.r3.u8 || !pdz_input::Supported())return;
  const auto player=input.r3.u32,output=input.r4.u32;
  if(!LocalPlayer(base,player)||!Readable(player,4536)||!Readable(output,12))return;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  if(!Readable(session,44)||REX_LOAD_U8(session)||REX_LOAD_U32(session+40)!=0)return;
  const auto collision=REX_LOAD_U32(player+452);
  if(!Readable(collision,52))return;
  std::array<uint32_t,3> saved{};
  for(unsigned i=0;i<3;++i)saved[i]=REX_LOAD_U32(output+4*i);
  auto position=input;
  sub_82151210(position,base);
  if(!Readable(position.r3.u32,12))return;
  const float start_y=std::bit_cast<float>(REX_LOAD_U32(position.r3.u32+4));
  auto fallback=input;
  {
    pdz_climb::ProbeScope scope(collision,start_y);
    // Retains native overhead, approach, top-surface and side-clearance rays.
    __imp__sub_824FBE20(fallback,base);
  }
  const float landing_y=std::bit_cast<float>(REX_LOAD_U32(output+4));
  const float rise=landing_y-start_y;
  bool finite=true;
  for(unsigned i=0;i<3;++i)finite &= std::isfinite(std::bit_cast<float>(REX_LOAD_U32(output+4*i)));
  // Joanna stands approximately 164 world units tall. Keep new climbs below
  // chest height; native authored climbs keep their original height limits.
  if(fallback.r3.u8 && finite && std::isfinite(rise) && rise>=pdz_climb::minimum_height && rise<=130.f){
    ctx=fallback;
    return;
  }
  for(unsigned i=0;i<3;++i)REX_STORE_U32(output+4*i,saved[i]);
}
