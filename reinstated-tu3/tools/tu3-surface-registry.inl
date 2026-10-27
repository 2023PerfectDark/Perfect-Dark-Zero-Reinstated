void RegisterSurface(PPCContext& ctx,uint8_t* base){
  if(!REXCVAR_GET(pdz_surface_mission_slot)||surface_registered||!pdz_input::Supported())return;
  constexpr uint32_t table=0x82D6C2B4;
  // TU3 allocates 300 slots and adds an availability byte array at +4.
  // Append to those arrays, preserving installed DLC and native ownership.
  const auto count=REX_LOAD_U32(table),solo_count=REX_LOAD_U32(table+20);
  if(count<14||count>=300||solo_count!=14){Trace("Surface slot rejected unexpected TU3 mission registry");return;}
  const auto available=REX_LOAD_U32(table+4),entries=REX_LOAD_U32(table+8);
  const auto definitions=REX_LOAD_U32(table+12),rules=REX_LOAD_U32(table+16),solo=REX_LOAD_U32(table+24);
  if(!Readable(available,300)||!Readable(entries,1200)||!Readable(definitions,1200)||
     !Readable(rules,1200)||!Readable(solo,1200))return;
  const auto host_mission=uint32_t(REXCVAR_GET(pdz_surface_host_mission));
  if(host_mission>=solo_count)return;
  const auto host_index=REX_LOAD_U32(solo+host_mission*4);
  if(host_index>=count)return;
  const auto host_entry=REX_LOAD_U32(entries+host_index*4);
  const auto host_definition=REX_LOAD_U32(definitions+host_index*4);
  if(!Readable(host_entry,8)||!Readable(host_definition,264))return;
  auto call=ctx;call.r1.u32-=512;if(!Readable(call.r1.u32,512))return;
  std::array<uint32_t,2> blocks{};
  for(unsigned i=0;i<2;++i){
    sub_8239DA98(call,base);call.r4.u64=call.r3.u32;call.r3.u64=i?264:8;
    sub_8239D740(call,base);blocks[i]=call.r3.u32;
    if(!blocks[i]){for(auto b:blocks)if(b){call.r3.u64=b;sub_8239D8A0(call,base);}return;}
  }
  std::memcpy(base+blocks[0],base+host_entry,8);
  std::memcpy(base+blocks[1],base+host_definition,264);
  REX_STORE_U8(blocks[0]+4,14);REX_STORE_U8(blocks[0]+5,0);
  REX_STORE_U32(blocks[1]+8,kSurfaceTextBase+5);
  REX_STORE_U32(blocks[1]+12,kSurfaceTextBase+6);
  REX_STORE_U8(blocks[1]+256,14);
  REX_STORE_U32(entries+count*4,blocks[0]);REX_STORE_U32(definitions+count*4,blocks[1]);
  REX_STORE_U32(rules+count*4,0);REX_STORE_U8(available+count,1);
  REX_STORE_U32(solo+14*4,count);
  REX_STORE_U32(table,count+1);REX_STORE_U32(table+20,15);
  surface_registered=true;
  Trace("Surface registered as native Mission 14 in TU3; campaign and DLC slots preserved");
}
