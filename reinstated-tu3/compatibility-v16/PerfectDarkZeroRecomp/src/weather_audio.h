#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace pdz_weather {
enum RainSurface { Ground,Water,Metal,Crate,Vehicle,Gutter,SurfaceCount };
// CC0 field recordings of rain on each surface (see RAIN_AUDIO_CREDITS.txt). All decoding is done before starting the
// stream; the callback only reads immutable samples and mixes bounded buffers.
class RainAudio {
 struct Layer {std::vector<float> pcm;size_t cursor=0;float gain=0;};
 std::array<Layer,SurfaceCount> layers_{};
 std::array<std::atomic<float>,SurfaceCount> target_{},pan_{};
 SDL_AudioStream* stream_=nullptr;
 bool initialized_=false,attempted_=false,paused_=true;
 float limiter_gain_=1.f;
 static void SDLCALL Callback(void* userdata,SDL_AudioStream* stream,int additional,int){
  auto& self=*static_cast<RainAudio*>(userdata);
  std::array<float,1024> samples{};
  while(additional>0){
   const int frames=std::min(additional/int(2*sizeof(float)),512);if(!frames)break;
   std::array<float,SurfaceCount> gain{},left{},right{};
   for(unsigned k=0;k<SurfaceCount;++k){
    gain[k]=self.target_[k].load(std::memory_order_relaxed);
    const float p=self.pan_[k].load(std::memory_order_relaxed);
    left[k]=std::sqrt((1-p)*.5f);right[k]=std::sqrt((1+p)*.5f);
   }
   for(int i=0;i<frames;++i){float l=0,r=0;
    for(unsigned k=0;k<SurfaceCount;++k){auto& s=self.layers_[k];
     s.gain+=(gain[k]-s.gain)*.00012f;
     if(s.pcm.empty())continue;
     const size_t fade=std::min(size_t(24000),s.pcm.size()/4);
     float v=s.pcm[s.cursor];
     // Crossfade loop boundaries, avoiding a periodic click or a hard cut.
     if(s.cursor>=s.pcm.size()-fade){
      const size_t j=s.cursor-(s.pcm.size()-fade);
      const float t=float(j)/float(fade);
      v=v*(1-t)+s.pcm[j]*t;
     }
     if(++s.cursor>=s.pcm.size())s.cursor=fade;
     v*=s.gain;l+=v*left[k];r+=v*right[k];
    }
    // +9 dB overall rain gain; linked stereo peak limiting preserves direction.
    l*=2.8183829f;r*=2.8183829f;
    const float peak=std::max(std::abs(l),std::abs(r));
    const float ceiling=peak>.9f?.9f/peak:1.f;
    if(ceiling<self.limiter_gain_)self.limiter_gain_=ceiling;
    else self.limiter_gain_+=(ceiling-self.limiter_gain_)*.0002f;
    samples[i*2]=l*self.limiter_gain_;samples[i*2+1]=r*self.limiter_gain_;
   }
   const int bytes=frames*2*sizeof(float);
   if(!SDL_PutAudioStreamData(stream,samples.data(),bytes))break;
   additional-=bytes;
  }
 }
 bool Load(){
  constexpr const char* files[]{"rain-ground.wav","rain-water.wav","rain-metal.wav","rain-wood.wav","rain-vehicle.wav","rain-gutter.wav"};
  for(unsigned k=0;k<SurfaceCount;++k){
   const auto filename=files[k];
   const auto path=std::string(SDL_GetBasePath())+"mod_assets/weather/audio/"+filename;
   SDL_AudioSpec source{};Uint8* bytes=nullptr;Uint32 length=0;
   if(!SDL_LoadWAV(path.c_str(),&source,&bytes,&length))return false;
   SDL_AudioSpec dest{SDL_AUDIO_F32,1,48000};Uint8* converted=nullptr;int count=0;
   const bool ok=SDL_ConvertAudioSamples(&source,bytes,int(length),&dest,&converted,&count);
   SDL_free(bytes);
   if(!ok||count<19200){SDL_free(converted);return false;}
   auto& pcm=layers_[k].pcm;
   pcm.resize(count/sizeof(float));std::memcpy(pcm.data(),converted,count);SDL_free(converted);
  }
  return true;
 }
 public:
 ~RainAudio(){if(stream_)SDL_DestroyAudioStream(stream_);if(initialized_)SDL_QuitSubSystem(SDL_INIT_AUDIO);}
 void Pause(){
  for(auto& value:target_)value.store(0,std::memory_order_relaxed);
  if(stream_&&!paused_){SDL_PauseAudioStreamDevice(stream_);SDL_ClearAudioStream(stream_);paused_=true;}
 }
 void Update(const std::array<float,SurfaceCount>& levels,const std::array<float,SurfaceCount>& pans,float volume){
  if(!attempted_){attempted_=true;initialized_=SDL_InitSubSystem(SDL_INIT_AUDIO);
   if(initialized_&&Load()){SDL_AudioSpec spec{SDL_AUDIO_F32,2,48000};stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,Callback,this);}}
  if(!stream_)return;
  constexpr std::array<float,SurfaceCount> scale{.65f,.55f,.65f,.70f,.65f,.55f};
  for(unsigned k=0;k<SurfaceCount;++k){target_[k].store(std::clamp(levels[k]*scale[k]*volume,0.f,1.f),std::memory_order_relaxed);pan_[k].store(std::clamp(pans[k],-.85f,.85f),std::memory_order_relaxed);}
  if(paused_){SDL_ResumeAudioStreamDevice(stream_);paused_=false;}
 }
};
}
