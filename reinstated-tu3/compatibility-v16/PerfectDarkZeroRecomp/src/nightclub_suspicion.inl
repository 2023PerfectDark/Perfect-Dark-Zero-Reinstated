// Runs on the same guest simulation thread as the population tracker.
struct SocialGuardState {
  pdz_nightclub::Suspicion exposure;
  bool greeted=false,aiming=false;
  float health=-1.f;
  unsigned voiced_stage=0;
};
std::map<uint32_t,SocialGuardState> suspicion;
std::chrono::steady_clock::time_point social_last{},next_conversation{},reply_at{};
bool reply_pending=false;
SDL_AudioStream* reply_stream=nullptr;
bool reply_audio_initialized=false;
void SocialReport(const char* gate,bool unarmed=false,unsigned nearby=0) {
  static auto next=std::chrono::steady_clock::time_point{};
  const auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::seconds(1);
  std::ofstream out("nightclub-suspicion-diagnostic.txt",std::ios::trunc);
  out<<"gate="<<gate<<" unarmed="<<unarmed<<" tracked="<<suspicion.size()<<" nearby="<<nearby<<'\n';
  for(const auto& [actor,state]:suspicion)out<<std::hex<<actor<<std::dec<<" seconds="<<state.exposure.seconds<<" stage="<<int(state.exposure.Stage())<<" health="<<state.health<<'\n';
}
bool SocialMission(uint8_t* base) {
  if(!pdz_input::Supported())return false;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  return Readable(session,40)&&REX_LOAD_U8(session+25)==1&&REX_LOAD_U32(session+36)<=1;
}
uint32_t SocialPlayer(uint8_t* base) {
  const auto owner=REX_LOAD_U32(0x82E35468);
  return Readable(owner,956)?REX_LOAD_U32(owner+952):0;
}
bool Unarmed(uint8_t* base,uint32_t actor) {
  if(!Readable(actor,4084))return false;
  for(unsigned offset:{4076u,4080u}) {
    const auto weapon=REX_LOAD_U32(actor+offset);
    if(!weapon)continue;
    if(!Readable(weapon,864))return false;
    const auto definition=REX_LOAD_U32(weapon+860);
    if(!Readable(definition,1148)||REX_LOAD_U32(definition+1144)!=0)return false;
  }
  return true;
}
bool SocialGuard(uint8_t* base,uint32_t actor) {
  if(!Readable(actor,13040))return false;
  const auto body=REX_LOAD_U32(actor+5280);
  if((body>=79&&body<=82)||body==7||body==113||body==30||body==99||body==11||body==12)return false;
  for(const auto& a:actors)if(a.address==actor)
    return Readable(a.setup,268)&&(REX_LOAD_U32(a.setup+184)||REX_LOAD_U32(a.setup+196));
  return false;
}
void StopReply() {
  if(reply_stream)SDL_DestroyAudioStream(reply_stream);
  reply_stream=nullptr;
  if(reply_audio_initialized)SDL_QuitSubSystem(SDL_INIT_AUDIO);
  reply_audio_initialized=false;
}
void PlayReply() {
  constexpr const char* names[]{"woman1_gen_s__0092","woman1_gen_s__0094","woman1_gen_s__0095",
    "woman1_gen_s__0104","woman1_gen_s__0105","woman2_gen_s__0132"};
  StopReply();
  const auto path=utils::RepoRoot()/"mod_assets/nightclub/greetings"/(std::string(names[placement_random()%6])+".wav");
  SDL_AudioSpec spec{};Uint8* data=nullptr;Uint32 size=0;
  if(!SDL_LoadWAV(path.string().c_str(),&spec,&data,&size))return;
  reply_audio_initialized=SDL_InitSubSystem(SDL_INIT_AUDIO);
  if(reply_audio_initialized)reply_stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
  if(reply_stream){SDL_PutAudioStreamData(reply_stream,data,int(size));SDL_FlushAudioStream(reply_stream);SDL_ResumeAudioStreamDevice(reply_stream);}
  SDL_free(data);
}
void GuardVoice(PPCContext call,uint8_t* base,uint32_t actor,const char* cue_name) {
  call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
  const auto name=call.r1.u32+400,cue=call.r1.u32+80;
  std::memcpy(base+name,cue_name,std::strlen(cue_name)+1);
  call.r3.u64=cue;call.r4.u64=name;sub_821FAC08(call,base);
  call.r3.u64=cue;call.r4.u64=actor+48;call.r6.u64=actor;call.r7.u64=1;call.f1.f64=1.;
  sub_821F7070(call,base);
}
void GuardAim(PPCContext call,uint8_t* base,uint32_t actor,uint32_t player,bool enabled) {
  call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
  auto point=Read(base,player+48);point.y+=135.f;Write(base,call.r1.u32+256,point);
  call.r3.u64=actor;call.r4.u64=enabled?call.r1.u32+256:0;sub_8251E6E8(call,base);
}
void GuardFace(PPCContext call,uint8_t* base,uint32_t actor,uint32_t player) {
  call.r1.u32-=2048;if(!Readable(call.r1.u32,2048))return;
  Write(base,call.r1.u32+256,Read(base,player+48));
  call.r3.u64=80;sub_82173980(call,base);const auto task=call.r3.u32;if(!task)return;
  call.r4.u64=call.r1.u32+256;call.r5.u64=1;call.r6.u64=0;call.r7.u64=0;
  sub_8219C8C0(call,base);call.r3.u64=actor+7912;call.r4.u64=task;sub_8217A530(call,base);
}
bool SocialSight(PPCContext call,uint8_t* base,uint32_t actor,uint32_t player) {
  call.r1.u32-=4096;if(!Readable(call.r1.u32,4096))return false;
  const auto from=call.r1.u32+256,to=from+16,hit=to+16;
  auto a=Read(base,actor+48),b=Read(base,player+48);a.y+=120.f;b.y+=120.f;
  Write(base,from,a);Write(base,to,b);REX_STORE_U32(hit,0);
  REX_STORE_U32(call.r1.u32+84,0);REX_STORE_U32(call.r1.u32+92,0);REX_STORE_U32(call.r1.u32+100,0);
  call.r3.u64=actor;call.r4.u64=from;call.r5.u64=to;call.r6.u64=4;
  call.r7.u64=hit;call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;
  sub_82239358(call,base);
  return !call.r3.u8||REX_LOAD_U32(hit)==player;
}
void SocialTick(PPCContext ctx,uint8_t* base) {
  if(!SocialMission(base)){social_last={};reply_pending=false;StopReply();return;}
  const auto session=REX_LOAD_U32(0x82D6B2B0),player=SocialPlayer(base);
  if(REX_LOAD_U8(session)||!Readable(player,4084)||!pdz_input::CrosshairVisible()) {
    social_last={};if(reply_stream)SDL_PauseAudioStreamDevice(reply_stream);SocialReport("paused-or-input");return;
  }
  if(reply_stream)SDL_ResumeAudioStreamDevice(reply_stream);
  const auto now=std::chrono::steady_clock::now();
  double elapsed=social_last.time_since_epoch().count()?std::chrono::duration<double>(now-social_last).count():0.;
  if(elapsed>0.&&elapsed<.05)return;
  social_last=now;elapsed=std::clamp(elapsed,0.,.15);
  const bool unarmed=Unarmed(base,player);
  if(reply_pending&&now>=reply_at){reply_pending=false;if(unarmed)PlayReply();}
  // Discover hostile guards using the existing relationship query; civilians
  // and named story actors never enter the suspicion map.
  for(const auto& a:actors)if(SocialGuard(base,a.address)&&!suspicion.contains(a.address)) {
    auto call=ctx;call.r1.u32-=2048;call.r3.u64=a.address;call.r4.u64=player;sub_825104D8(call,base);
  }
  const auto position=Read(base,player+48);
  unsigned nearby_count=0;
  for(auto& [actor,state]:suspicion) {
    if(!Readable(actor,13040))continue;
    auto health=ctx;health.r1.u32-=2048;health.r3.u64=actor;health.r4.u64=0;
    const auto table=REX_LOAD_U32(actor);if(!Readable(table,336))continue;
    const auto fn=REX_LOAD_U32(table+332);
    if(fn<0x82100000||fn>=0x82D00000)continue;
    {auto ctx=health;REX_CALL_INDIRECT_FUNC(fn);health=ctx;}
    const float hp=float(health.f1.f64);if(!std::isfinite(hp)||hp<=0)continue;
    const bool attacked=state.health>=0&&hp<state.health;state.health=hp;
    const auto p=Read(base,actor+48);
    bool nearby=Finite(p)&&Finite(position)&&std::hypot(p.x-position.x,p.z-position.z)<=300.f&&std::abs(p.y-position.y)<=180.f;
    if(nearby)nearby=SocialSight(ctx,base,actor,player);
    if(nearby)++nearby_count;
    const auto previous=state.exposure.Stage();
    const auto stage=state.exposure.Update(elapsed,nearby,unarmed,true,attacked);
    const bool aim=stage==pdz_nightclub::SuspicionStage::Aiming&&nearby&&unarmed;
    if(aim&&!state.aiming)GuardFace(ctx,base,actor,player);
    if(aim||state.aiming)GuardAim(ctx,base,actor,player,aim);
    state.aiming=aim;
    if(stage!=previous)REXLOG_INFO("Nightclub suspicion: actor {:08X}, stage {}, exposure {:.2f}",actor,int(stage),state.exposure.seconds);
    if(stage==pdz_nightclub::SuspicionStage::Hostile)continue;
    if(!nearby||!unarmed||now<next_conversation)continue;
    if(stage==pdz_nightclub::SuspicionStage::Greeting&&!state.greeted){
      GuardVoice(ctx,base,actor,"XACT_SOUNDBANK_GANGSTA2_SEE_PLAYER_PARTY");state.greeted=true;
      reply_pending=true;reply_at=now+std::chrono::seconds(2);next_conversation=now+std::chrono::seconds(4);
    }else if(unsigned(stage)>state.voiced_stage){
      GuardVoice(ctx,base,actor,stage==pdz_nightclub::SuspicionStage::Suspicious?
        "XACT_SOUNDBANK_GANGSTA2_PLAYER_CLUB_FRONT_WARN":"XACT_SOUNDBANK_GANGSTA2_PLAYER_IGNORES_WARNING");
      next_conversation=now+std::chrono::seconds(3);
      state.voiced_stage=unsigned(stage);
    }
  }
  SocialReport("running",unarmed,nearby_count);
}
