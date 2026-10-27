#pragma once
#include <SDL3/SDL.h>
#include <rex/logging.h>
#include "utils.h"
#include <array>
#include <vector>
#include <string_view>
#include <mutex>
namespace pdz_heels {
inline std::string_view CueName(std::string_view cue){
  constexpr std::string_view bank="xact_soundbank_stock_";
  if(cue.starts_with(bank))cue.remove_prefix(bank.size());
  return cue;
}
inline bool Eligible(unsigned body,std::string_view cue){
  cue=CueName(cue);
  if(body!=80&&body!=82&&body!=109)return false;
  if(!cue.starts_with("fstep_"))return false;
  return (cue.ends_with("_walk")||cue.ends_with("_run"))&&
    (cue.starts_with("fstep_conc_")||cue.starts_with("fstep_marble_")||
     cue.starts_with("fstep_wood_")||cue.starts_with("fstep_creaky_")||cue.starts_with("fstep_metal_"));
}
class Audio {
 std::mutex mutex_;bool attempted_=false,initialized_=false;
 SDL_AudioStream* stream_=nullptr;
 std::array<std::vector<unsigned char>,16> clips_{};unsigned next_=0;
 public:
 ~Audio(){if(stream_)SDL_DestroyAudioStream(stream_);if(initialized_)SDL_QuitSubSystem(SDL_INIT_AUDIO);}
 bool Play(float volume,bool concrete){
  std::lock_guard lock(mutex_);
  if(!attempted_){
   attempted_=true;
   for(unsigned i=0;i<clips_.size();++i){
    auto path=(utils::RepoRoot()/"mod_assets/footsteps"/((i<8?"heel-":"concrete-heel-")+std::to_string(i%8)+".wav")).string();
    SDL_AudioSpec spec{};Uint8* data=nullptr;Uint32 length=0;
    if(!SDL_LoadWAV(path.c_str(),&spec,&data,&length)){REXLOG_ERROR("Heel audio missing: {}",path);return false;}
    const bool valid=spec.freq==48000&&spec.channels==1&&spec.format==SDL_AUDIO_S16LE&&length<96000;
    if(valid)clips_[i].assign(data,data+length);SDL_free(data);if(!valid)return false;
   }
   initialized_=SDL_InitSubSystem(SDL_INIT_AUDIO);if(!initialized_)return false;
   SDL_AudioSpec spec{SDL_AUDIO_S16LE,1,48000};
   stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
   if(stream_)REXLOG_INFO("Dress heel footsteps ready: eight footfall variations per surface set");
  }
  if(!stream_)return false;
  // Never queue a walking loop or accumulate steps behind the player's gait.
  SDL_ClearAudioStream(stream_);SDL_SetAudioStreamGain(stream_,volume*.85f);
  auto& clip=clips_[(concrete?8:0)+(next_++%8)];
  return SDL_PutAudioStreamData(stream_,clip.data(),int(clip.size()))&&SDL_FlushAudioStream(stream_)&&SDL_ResumeAudioStreamDevice(stream_);
 }
};
inline Audio audio;
}
