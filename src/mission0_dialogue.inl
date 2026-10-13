// Mission 0 elevator position captured before the scientist boards.
// Keep this in the player-update thread alongside campaign mission resets.
namespace pdz_mission0 {
std::atomic<uint32_t> rideRequest{0};
void PollRide(uint32_t player,bool held){
  static bool previous=false;
  if(held&&!previous)rideRequest.store(player);
  previous=held;
}
}
namespace {
SDL_AudioStream* elevatorVoice=nullptr;
bool elevatorVoicePlayed=false;
void ResetElevatorVoice(){
  if(elevatorVoice){SDL_DestroyAudioStream(elevatorVoice);elevatorVoice=nullptr;}
  elevatorVoicePlayed=false;
  pdz_mission0::rideRequest.store(0);
}
void TickElevatorVoice(uint8_t* base,uint32_t player){
  if(!pdz_input::Supported()||!LocalPlayer(base,player)||!Readable(player,60))return;
  const bool ridePressed=pdz_mission0::rideRequest.exchange(0)==player;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  if(!Readable(session,40)||REX_LOAD_U8(session+25)!=0||REX_LOAD_U32(session+36)>1){
    if(elevatorVoice){SDL_DestroyAudioStream(elevatorVoice);elevatorVoice=nullptr;}
    return;
  }
  // Retain the stream until mission reset: an empty source queue can still
  // have its final samples buffered on the playback device.
  if(elevatorVoicePlayed||!ridePressed)return;
  const float x=ReadFloat(base,player+48),y=ReadFloat(base,player+52),z=ReadFloat(base,player+56);
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;
  // A small volume on the elevator, not a timer from mission start.
  if(std::abs(x+8367.39f)>140.f||std::abs(z-7519.04f)>140.f||std::abs(y-1005.03f)>100.f)return;
  // Reject the interaction when an NPC is already aboard. Fail closed if
  // the native actor list cannot be inspected safely.
  const auto manager=REX_LOAD_U32(0x82D6B2B0+24);
  if(!Readable(manager,43188))return;
  auto node=REX_LOAD_U32(manager+43180);const auto end=REX_LOAD_U32(manager+43184);
  unsigned visited=0;
  for(;node!=end&&visited<4096;++visited){
    if(!Readable(node,16))return;
    const auto actor=REX_LOAD_U32(node+4);node=REX_LOAD_U32(node+12);
    if(!Readable(actor,60))return;
    if(REX_LOAD_U32(actor+12)!=33)continue;
    const float ax=ReadFloat(base,actor+48),ay=ReadFloat(base,actor+52),az=ReadFloat(base,actor+56);
    if(!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az))return;
    if(std::abs(ax+8367.39f)<200.f&&std::abs(az-7519.04f)<200.f&&std::abs(ay-1005.03f)<180.f)return;
  }
  if(node!=end)return;
  elevatorVoicePlayed=true;
  SDL_AudioSpec spec{};Uint8* samples=nullptr;Uint32 length=0;
  if(!SDL_LoadWAV("mod_assets/mission0/scientist-elevator.wav",&spec,&samples,&length)){
    REXLOG_ERROR("Mission 0 elevator voice load failed: {}",SDL_GetError());return;
  }
  elevatorVoice=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
  const bool queued=elevatorVoice&&SDL_PutAudioStreamData(elevatorVoice,samples,int(length));
  SDL_free(samples);
  if(!queued||!SDL_FlushAudioStream(elevatorVoice)||!SDL_ResumeAudioStreamDevice(elevatorVoice)){
    REXLOG_ERROR("Mission 0 elevator voice playback failed: {}",SDL_GetError());
    if(elevatorVoice){SDL_DestroyAudioStream(elevatorVoice);elevatorVoice=nullptr;}return;
  }
  REXLOG_INFO("Mission 0 scientist elevator line triggered at {}, {}, {}",x,y,z);
}
}
