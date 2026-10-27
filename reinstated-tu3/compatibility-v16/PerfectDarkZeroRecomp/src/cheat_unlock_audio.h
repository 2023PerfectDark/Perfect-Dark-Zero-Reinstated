#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <rex/logging.h>
namespace pdz_cheats {
// Host UI owns this one-shot stream. It never replaces mission music.
class UnlockAudio {
 public:
  ~UnlockAudio() {if(stream_)SDL_DestroyAudioStream(stream_);if(initialized_)SDL_QuitSubSystem(SDL_INIT_AUDIO);}
  void Configure(std::filesystem::path path) {path_=std::move(path);}
  void Play() {
    if(path_.empty())return;
    if(!initialized_) {initialized_=SDL_InitSubSystem(SDL_INIT_AUDIO);if(!initialized_)return;}
    if(stream_) {SDL_DestroyAudioStream(stream_);stream_=nullptr;}
    SDL_AudioSpec spec{};Uint8* data=nullptr;Uint32 size=0;
    if(!SDL_LoadWAV(path_.string().c_str(),&spec,&data,&size)) {
      REXLOG_WARN("Cheat unlock sound unavailable: {}",SDL_GetError());return;
    }
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(stream_) {
      if(SDL_PutAudioStreamData(stream_,data,int(size))) {
        SDL_FlushAudioStream(stream_);SDL_ResumeAudioStreamDevice(stream_);
      }
    }
    SDL_free(data);
  }
 private:
  std::filesystem::path path_;
  SDL_AudioStream* stream_=nullptr;
  bool initialized_=false;
};
inline UnlockAudio unlock_audio;
}
