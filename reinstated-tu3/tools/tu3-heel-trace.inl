  // Bounded trace includes rejected candidates, not only successful playback.
  static std::atomic<unsigned> heel_trace_count{0};
  if(heel_trace_count.load()<80){
    std::string trace_name;
    for(unsigned i=0;i<96&&Readable(ctx.r3.u32+i,1);++i){char c=char(base[ctx.r3.u32+i]);if(!c)break;trace_name+=c;}
    if(trace_name.find("step")!=std::string::npos||trace_name.find("STEP")!=std::string::npos){
      const auto camera=REX_LOAD_U32(0x82D69048);
      const auto player=Readable(camera,12)?REX_LOAD_U32(camera+8):0;
      if(heel_trace_count.fetch_add(1)<80)REXLOG_INFO("Heel trace: supported={} cue={} owner={:08X} player={:08X} arg4={:08X} arg5={:08X} playerBody={} ownerBody={}",pdz_input::Supported(),trace_name,native_footstep_actor,player,ctx.r4.u32,ctx.r5.u32,Readable(player,5284)?REX_LOAD_U32(player+5280):0xffffffffu,Readable(native_footstep_actor,5284)?REX_LOAD_U32(native_footstep_actor+5280):0xffffffffu);
    }
  }
