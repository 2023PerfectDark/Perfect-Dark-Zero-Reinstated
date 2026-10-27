// Native exact crosshair ray, sampled on the guest simulation thread.
pdz_disco::Dwell disco_dwell;
MissionMusic disco_music;
std::chrono::steady_clock::time_point disco_last{};
void ResetDisco(){disco_music.Stop();disco_dwell.Reset();disco_last={};}
void TickDisco(PPCContext ctx,uint8_t* base){
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  if(!pdz_input::Supported()||!Readable(session,44)||REX_LOAD_U8(session+25)!=1||REX_LOAD_U32(session+40)>1){
    if(disco_music.stream)SetNativeMusicMuted(ctx,base,false);
    ResetDisco();return;
  }
  auto now=std::chrono::steady_clock::now();
  if(REX_LOAD_U8(session)||!pdz_input::CrosshairVisible()){
    disco_last={};disco_dwell.seconds=0;disco_music.Pause();
    if(disco_music.stream)SetNativeMusicMuted(ctx,base,false);
    return;
  }
  if(disco_dwell.triggered){
    disco_music.Tick();SetNativeMusicMuted(ctx,base,disco_music.stream!=nullptr);return;
  }
  double elapsed=disco_last.time_since_epoch().count()?std::chrono::duration<double>(now-disco_last).count():0;
  if(elapsed>0&&elapsed<.05)return;
  disco_last=now;
  const auto camera=REX_LOAD_U32(0x82D69048);
  bool focused=false;
  if(Readable(camera,0x540)){
    const auto player=REX_LOAD_U32(camera+8),view=REX_LOAD_U32(camera+60),mode=REX_LOAD_U32(camera+0x53C);
    ctx.r1.u32-=2048;
    if(Readable(player,96)&&Readable(view,240)&&Readable(mode,12)&&REX_LOAD_U8(mode+11)<=1&&Readable(ctx.r1.u32,2048)){
      const auto from=ctx.r1.u32+256,to=ctx.r1.u32+272,out=ctx.r1.u32+288;
      bool finite=true;
      for(unsigned i=0;i<3;++i){
        float origin=std::bit_cast<float>(REX_LOAD_U32(view+224+i*4));
        float forward=-std::bit_cast<float>(REX_LOAD_U32(view+208+i*4));
        finite=finite&&std::isfinite(origin)&&std::isfinite(forward);
        REX_STORE_U32(from+i*4,std::bit_cast<uint32_t>(origin));
        REX_STORE_U32(to+i*4,std::bit_cast<uint32_t>(origin+forward*15000.f));
      }
      if(finite){
        REX_STORE_U32(out,0);REX_STORE_U32(ctx.r1.u32+84,0);REX_STORE_U32(ctx.r1.u32+92,0);REX_STORE_U32(ctx.r1.u32+100,0);
        ctx.r3.u64=player;ctx.r4.u64=from;ctx.r5.u64=to;ctx.r6.u64=4;
        ctx.r7.u64=out;ctx.r8.u64=0;ctx.r9.u64=0;ctx.r10.u64=0;
        sub_82240620(ctx,base);
        const auto prop=REX_LOAD_U32(out);
        if(ctx.r3.u8&&Readable(prop,96)&&REX_LOAD_U32(prop+12)==11){
          auto model=REX_LOAD_U32(prop+72);
          auto descriptor=Readable(model,4)?REX_LOAD_U32(model):0;
          auto name=Readable(descriptor,4)?REX_LOAD_U32(descriptor):0;
          constexpr char expected[]="aid_scenegraph_xenon_props_niknak_discoball";
          focused=Readable(name,sizeof(expected))&&!std::memcmp(base+name,expected,sizeof(expected));
        }
      }
    }
  }
  if(disco_dwell.Update(focused,elapsed)){
    disco_music.path="mod_assets/nightclub/disco-loop.wav";
    disco_music.disco=true;
    disco_music.Tick();SetNativeMusicMuted(ctx,base,disco_music.stream!=nullptr);
    Trace(disco_music.stream?"Disco crosshair Easter egg started after 3 seconds":"Disco Easter egg audio unavailable");
  }
}
