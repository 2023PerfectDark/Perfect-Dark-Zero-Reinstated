  if(step){
    const auto camera=REX_LOAD_U32(0x82D69048);
    const auto player=Readable(camera,12)?REX_LOAD_U32(camera+8):0;
    std::string name;
    for(unsigned i=0;i<64&&Readable(ctx.r3.u32+i,1);++i){char c=char(base[ctx.r3.u32+i]);if(!c)break;if(c>='A'&&c<='Z')c+=32;name+=c;}
    // Campaign costume swaps replace the bound definition but retain Joanna's
    // original body ID at +5280. Use the actual bound model at +5264.
    const auto definition=Readable(player,5284)?REX_LOAD_U32(player+5264):0;
    const auto body=Readable(definition,4)?REX_LOAD_U32(definition):0xffffffffu;
    if(pdz_heels::Eligible(body,name)){
      auto volume=ctx;volume.r1.u32-=512;float gain=1.f;
      if(Readable(volume.r1.u32-16,528)){
        volume.r3.u64=13;volume.r4.u64=REX_LOAD_U8(0x82E67C48+13*32+28);
        sub_821FC158(volume,base);gain=std::clamp(float(volume.r3.u32)/255.f,0.f,1.f);
      }
      if(pdz_heels::audio.Play(gain,pdz_heels::CueName(name).starts_with("fstep_conc_"))){
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto previous=last_footstep_ms.load();
        if(now-previous>80&&last_footstep_ms.compare_exchange_strong(previous,now))footstep_sequence.fetch_add(1,std::memory_order_relaxed);
        // No native sound object was allocated. Caller skips sound-object setup;
        // its separate gameplay footstep/noise calculation remains native.
        ctx.r3.u64=0;return;
      }
    }
  }
