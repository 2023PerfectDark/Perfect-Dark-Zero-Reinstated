#include "death_voice.h"
#include "death_voice_policy.h"
#include "campaign_gore.h"
#include "generated/default/pdzstocktu3_init.h"
#include "mousehook.h"
#include "game_cvars.h"
#include "utils.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <mutex>
#include <random>
#include <vector>

REX_EXTERN(__imp__sub_8262CAE0);
REX_EXTERN(__imp__sub_8262AB60);
REX_EXTERN(__imp__sub_821FF3C8);
namespace pdz_death_voice {
namespace {
using Clock=std::chrono::steady_clock;
using Vec=std::array<float,3>;
bool Readable(uint32_t a,uint32_t bytes){
 auto* rt=rex::Runtime::instance();if(!rt||!a||uint64_t(a)+bytes>0x100000000ull)return false;
 for(uint64_t p=a;p<uint64_t(a)+bytes;){auto* heap=rt->memory()->LookupHeap(uint32_t(p));uint32_t flags=0;
  if(!heap||!heap->QueryProtect(uint32_t(p),&flags)||!(flags&rex::memory::kMemoryProtectRead))return false;
  p=(p/heap->page_size()+1)*heap->page_size();}return true;
}
int Slot(uint32_t model){return model>=0x82E75160&&(model-0x82E75160)%512==0&&(model-0x82E75160)/512<2500?int((model-0x82E75160)/512):-1;}
Vec Read(uint8_t* base,uint32_t p){Vec v{};for(unsigned i=0;i<3;++i)v[i]=std::bit_cast<float>(REX_LOAD_U32(p+i*4));return v;}
bool Finite(const Vec& v){return std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x);});}
std::string Text(uint8_t* base,uint32_t p){
 std::string out;for(unsigned i=0;i<256&&Readable(p+i,1);++i){char v=char(base[p+i]);if(!v)break;
  if(v<' '||v>'~')return {};if(v>='A'&&v<='Z')v+=32;out+=v;}return out;
}
class Audio {
 struct Voice {unsigned clip=0;size_t cursor=0;uint32_t model=0;float left=0,right=0;};
 std::array<std::vector<float>,death_voice::clips.size()> clips_;
 std::array<Voice,32> voices_{};
 std::mutex mutex_;SDL_AudioStream* stream_=nullptr;bool initialized_=false;int language_=-1;
 std::atomic<bool> paused_{true};
 static void SDLCALL Callback(void* userdata,SDL_AudioStream* stream,int additional,int){
  auto& self=*static_cast<Audio*>(userdata);std::array<float,1024> pcm{};
  while(additional>=int(2*sizeof(float))){const int frames=std::min(additional/int(2*sizeof(float)),512);pcm.fill(0);
   {std::lock_guard lock(self.mutex_);
    if(!self.paused_.load(std::memory_order_relaxed))for(auto& voice:self.voices_){
     if(!voice.model)continue;const auto& data=self.clips_[voice.clip];
     for(int i=0;i<frames&&voice.cursor<data.size();++i){const float v=data[voice.cursor++];pcm[i*2]+=v*voice.left;pcm[i*2+1]+=v*voice.right;}
     if(voice.cursor>=data.size())voice={};
    }
   }
   for(auto& v:pcm)v=std::clamp(v,-.98f,.98f);
   const int bytes=frames*2*sizeof(float);if(!SDL_PutAudioStreamData(stream,pcm.data(),bytes))break;additional-=bytes;
  }
 }
 public:
 ~Audio(){if(stream_)SDL_DestroyAudioStream(stream_);if(initialized_)SDL_QuitSubSystem(SDL_INIT_AUDIO);}
 bool Ready()const{return stream_!=nullptr;}
 void Prepare(){
  const int language=std::clamp(REXCVAR_GET(pdz_dub_language),0,6);if(language_==language&&Ready())return;
  if(stream_){SDL_DestroyAudioStream(stream_);stream_=nullptr;}voices_={};language_=language;
  constexpr const char* languages[]{"english","german","japanese","latam","spanish","french","italian"};
  unsigned loaded=0;
  // Only load at the mission reset. Headshot callbacks do no file I/O or decoding.
  for(unsigned i=0;i<clips_.size();++i){clips_[i].clear();
   auto path=utils::RepoRoot()/"mod_assets/death-voices"/languages[language]/(std::string(death_voice::clips[i])+".wav");
   if(!std::filesystem::exists(path))path=utils::RepoRoot()/"mod_assets/death-voices/english"/(std::string(death_voice::clips[i])+".wav");
   SDL_AudioSpec spec{};Uint8* bytes=nullptr;Uint32 size=0;
   if(!SDL_LoadWAV(path.string().c_str(),&spec,&bytes,&size)){
    REXLOG_ERROR("Death voice load failed: {}: {}",path.string(),SDL_GetError());continue;
   }
   SDL_AudioSpec target{SDL_AUDIO_F32,1,48000};Uint8* converted=nullptr;int count=0;
   if(size<16*1024*1024&&SDL_ConvertAudioSamples(&spec,bytes,int(size),&target,&converted,&count)){
    clips_[i].resize(count/sizeof(float));std::memcpy(clips_[i].data(),converted,clips_[i].size()*sizeof(float));SDL_free(converted);++loaded;
   }SDL_free(bytes);
  }
  if(loaded!=clips_.size()){REXLOG_ERROR("Death voice clips incomplete: {}/{}",loaded,clips_.size());return;}
  if(!initialized_)initialized_=SDL_InitSubSystem(SDL_INIT_AUDIO);if(!initialized_)return;
  SDL_AudioSpec stereo{SDL_AUDIO_F32,2,48000};
  stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&stereo,Callback,this);
  if(stream_&&!SDL_ResumeAudioStreamDevice(stream_)){SDL_DestroyAudioStream(stream_);stream_=nullptr;}
  REXLOG_INFO("Headshot death voices: {} clips, playback={}",loaded,Ready());
 }
 void Pause(bool pause){paused_.store(pause,std::memory_order_relaxed);}
 void Clear(){std::lock_guard lock(mutex_);voices_={};paused_=true;}
 void Stop(uint32_t model){std::lock_guard lock(mutex_);for(auto& v:voices_)if(v.model==model)v={};}
 void Transfer(uint32_t a,uint32_t b){std::lock_guard lock(mutex_);for(auto& v:voices_)if(v.model==a)v.model=b;}
 void Gains(uint32_t model,float gain,float pan){std::lock_guard lock(mutex_);for(auto& v:voices_)if(v.model==model){v.left=gain*std::sqrt((1-pan)*.5f);v.right=gain*std::sqrt((1+pan)*.5f);}}
 void Play(unsigned clip,uint32_t model){
  std::lock_guard lock(mutex_);auto slot=std::find_if(voices_.begin(),voices_.end(),[](const auto& v){return !v.model;});
  if(slot==voices_.end())slot=std::max_element(voices_.begin(),voices_.end(),[](const auto& a,const auto& b){return a.cursor<b.cursor;});
  *slot={clip,0,model,0,0};
 }
} audio;
// Keep the death route for the model lifetime, including delayed corpse cues.
struct Identity {uint32_t actor=0,definition=0;std::string_view bank;bool fatalHeadshot=false;};
std::array<Identity,2500> identities{};
struct Death {uint32_t actor=0,model=0,definition=0,body=0;std::string_view bank;Vec position{};Clock::time_point ready{};bool played=false;};
std::array<Death,64> deaths{};
enum class CuePath { Speech, OneShot };
struct DeferredCue {
 CuePath path=CuePath::Speech;
 uint32_t speech=0,positional=0,actor=0,flags=0;
 double gain=1;
 // Native records include the resolved cue ID after the 256-byte name.
 std::array<uint8_t,260> record{};
 std::array<uint8_t,12> position{};
 std::string name;
};
struct PendingHit {uint32_t actor=0;std::vector<DeferredCue> cues;};
thread_local std::vector<PendingHit> hitActors;
thread_local uint32_t nativeDeathActor=0;
std::mt19937 random{std::random_device{}()};
auto nextTick=Clock::time_point{};
}
void Begin(const Shot& shot){if(shot.actor)hitActors.push_back({shot.actor,{}});}
void End(PPCContext ctx,uint8_t* base,const Shot& shot,float after,bool headRemoved){
 if(!shot.actor)return;
 const bool fatal=death_voice::Fatal(shot.region,shot.health,after,false);
 if(!hitActors.empty()){
  auto pending=std::move(hitActors.back());hitActors.pop_back();
  // Native speech is requested inside damage processing, sometimes before health
  // is updated. Replay it only when the head hit was nonfatal (or audio failed).
  if(!fatal||!audio.Ready())for(const auto& cue:pending.cues){
   auto call=ctx;if(call.r1.u32<1024||!Readable(call.r1.u32-1024,1024))continue;
   call.r1.u32-=1024;const auto record=call.r1.u32+512,position=call.r1.u32+784;
   std::memcpy(base+record,cue.record.data(),cue.record.size());
   call.r6.u64=cue.actor;call.r7.u64=cue.flags;
   if(cue.path==CuePath::OneShot){
    std::memcpy(base+position,cue.position.data(),cue.position.size());
    call.r3.u64=record;call.r4.u64=cue.positional?position:0;call.f1.f64=cue.gain;
    __imp__sub_821FF3C8(call,base);
   }else{
    call.r3.u64=cue.speech;call.r4.u64=record;call.r5.u64=cue.positional;
    __imp__sub_8262CAE0(call,base);
   }
  }
  else for(const auto& cue:pending.cues)
   REXLOG_INFO("Discarded native headshot death cue: actor={:08X} path={} cue={}",shot.actor,cue.path==CuePath::OneShot?"direct":"speech",cue.name);
 }
 const int slot=Slot(shot.model);
 if(fatal&&slot>=0&&Readable(shot.model,4)){
  auto& identity=identities[slot];const auto definition=REX_LOAD_U32(shot.model);
  if(identity.actor!=shot.actor||identity.definition!=definition)identity={shot.actor,definition,{}};
  identity.fatalHeadshot=true;
  REXLOG_INFO("Fatal headshot voice route: actor={:08X} head_removed={} ready={}",shot.actor,headRemoved,audio.Ready());
 }
 if(!audio.Ready()||!death_voice::Fatal(shot.region,shot.health,after,headRemoved)||!Readable(shot.actor,76)||!Readable(shot.model,4))return;
 if(std::any_of(deaths.begin(),deaths.end(),[&](const auto& d){return d.actor==shot.actor&&d.model==shot.model;}))return;
 auto it=std::find_if(deaths.begin(),deaths.end(),[](const auto& d){return !d.actor;});
 if(it==deaths.end())it=std::min_element(deaths.begin(),deaths.end(),[](const auto& a,const auto& b){return a.ready<b.ready;});
 if(it->model)audio.Stop(it->model);
 const auto definition=REX_LOAD_U32(shot.model);
 std::string_view bank;
 if(slot>=0&&identities[slot].actor==shot.actor&&identities[slot].definition==definition)bank=identities[slot].bank;
 *it={shot.actor,shot.model,definition,shot.body,bank,Read(base,shot.actor+48),Clock::now()+std::chrono::milliseconds(180),false};
}
void Reset(){deaths={};identities={};audio.Clear();audio.Prepare();nextTick={};}
void Forget(uint32_t model){
 audio.Stop(model);for(auto& d:deaths)if(d.model==model)d={};const int slot=Slot(model);if(slot>=0)identities[slot]={};
}
void Transfer(uint8_t* base,uint32_t actor,uint32_t corpse){
 if(!Readable(corpse,76))return;const auto model=REX_LOAD_U32(corpse+72);if(!Readable(model,4))return;
 const int target=Slot(model);
 if(target>=0)for(auto& identity:identities)if(identity.actor==actor){
  const auto copy=identity;identities[target]={corpse,REX_LOAD_U32(model),copy.bank,copy.fatalHeadshot};break;
 }
 for(auto& d:deaths)if(d.actor==actor){audio.Transfer(d.model,model);d.actor=corpse;d.model=model;d.definition=REX_LOAD_U32(model);}
}
void Tick(PPCContext ctx,uint8_t* base){
 if(!pdz_input::Supported())return;const auto session=REX_LOAD_U32(0x82D6BFD8);
 if(!Readable(session,44))return;
 const bool pause=REX_LOAD_U8(session)!=0||REXCVAR_GET(pdz_menu_mode);audio.Pause(pause);if(pause)return;
 const auto now=Clock::now();if(now<nextTick)return;nextTick=now+std::chrono::milliseconds(50);
 const auto camera=REX_LOAD_U32(0x82D69048);if(!Readable(camera,64))return;
 const auto view=REX_LOAD_U32(camera+60);if(!Readable(view,240))return;
 const auto eye=Read(base,view+224),right=Read(base,view+176);if(!Finite(eye)||!Finite(right))return;
 float volume=1.f;auto call=ctx;if(call.r1.u32>=512&&Readable(call.r1.u32-512,512)){
  call.r1.u32-=512;call.r3.u64=13;call.r4.u64=REX_LOAD_U8(0x82E67C48+13*32+28);sub_821FC158(call,base);volume=std::clamp(float(call.r3.u32)/255.f,0.f,1.f);
 }
 for(auto& d:deaths){if(!d.actor)continue;
  if(!Readable(d.model,4)||REX_LOAD_U32(d.model)!=d.definition||(pdz_gore::RemovedParts(base,d.model)&1)||now-d.ready>std::chrono::seconds(30)){
   audio.Stop(d.model);d={};continue;
  }
  if(Readable(d.actor,76)&&REX_LOAD_U32(d.actor+72)==d.model)d.position=Read(base,d.actor+48);
  if(!Finite(d.position)){audio.Stop(d.model);d={};continue;}
  if(!d.played&&now>=d.ready){const unsigned clip=death_voice::Choose(d.bank,d.body,random());audio.Play(clip,d.model);d.played=true;
   REXLOG_INFO("Headshot death voice: actor={:08X} voice={} clip={}",d.actor,d.bank.empty()?"fallback":d.bank,death_voice::clips[clip]);}
  float distance2=0,dot=0;for(unsigned k=0;k<3;++k){const float delta=d.position[k]-eye[k];distance2+=delta*delta;dot+=delta*right[k];}
  const float distance=std::sqrt(distance2),pan=std::clamp(dot/std::max(100.f,distance),-1.f,1.f);
  const float gain=volume/(1.f+distance2/(600.f*600.f));audio.Gains(d.model,gain,pan);
 }
}
// Native speech dispatcher supplies the actual assigned voice bank and owner.
// Observe it rather than deriving an elite/gangster voice from the outfit.
bool Observe(PPCContext ctx,uint8_t* base,CuePath path){
 if(!pdz_input::Supported())return false;const auto actor=ctx.r6.u32;if(!Readable(actor,76))return false;
 const auto model=REX_LOAD_U32(actor+72);const int slot=Slot(model);if(slot<0||!Readable(model,4))return false;
 const auto record=path==CuePath::OneShot?ctx.r3.u32:ctx.r4.u32;
 const auto cue=Text(base,record);const auto bank=death_voice::Bank(cue);
 auto& identity=identities[slot];const auto definition=REX_LOAD_U32(model);
 if(identity.actor!=actor||identity.definition!=definition)identity={actor,definition,{}};
 if(!bank.empty()){identity.bank=bank;for(auto& d:deaths)if(d.actor==actor)d.bank=bank;}
 if(!audio.Ready())return false;
 auto pending=std::find_if(hitActors.rbegin(),hitActors.rend(),[&](const auto& hit){return hit.actor==actor;});
 const bool nativeDeath=path==CuePath::OneShot&&nativeDeathActor==actor;
 const auto route=death_voice::RouteDeath(cue,pending!=hitActors.rend(),identity.fatalHeadshot,nativeDeath);
 if(route==death_voice::Route::Defer){
  if(!Readable(record,260))return false;
  DeferredCue saved;saved.path=path;saved.actor=actor;saved.flags=ctx.r7.u32;saved.name=cue;
  std::memcpy(saved.record.data(),base+record,saved.record.size());
  if(path==CuePath::OneShot){
   saved.positional=ctx.r4.u32;saved.gain=ctx.f1.f64;
   if(saved.positional){if(!Readable(saved.positional,12))return false;
    std::memcpy(saved.position.data(),base+saved.positional,saved.position.size());}
  }else{saved.speech=ctx.r3.u32;saved.positional=ctx.r5.u32;}
  pending->cues.push_back(std::move(saved));return true;
 }
 if(route==death_voice::Route::Suppress){
  REXLOG_INFO("Suppressed old headshot death cue: actor={:08X} cue={}",actor,cue);return true;
 }
 return false;
}
void NativeDeath(PPCContext& ctx,uint8_t* base){
 // Run the native cleanup/voice selection, but identify its direct audio call.
 // Suppressing the whole routine would leave the actor's previous speech alive.
 struct Scope {uint32_t previous;~Scope(){nativeDeathActor=previous;}} scope{nativeDeathActor};
 nativeDeathActor=pdz_input::Supported()&&Readable(ctx.r3.u32,164)?REX_LOAD_U32(ctx.r3.u32+160):0;
 __imp__sub_8262AB60(ctx,base);
}
}
REX_HOOK_RAW(sub_8262CAE0){
 if(pdz_death_voice::Observe(ctx,base,pdz_death_voice::CuePath::Speech)){ctx.r3.u64=0;return;}
 __imp__sub_8262CAE0(ctx,base);
}
REX_HOOK_RAW(sub_8262AB60){pdz_death_voice::NativeDeath(ctx,base);}
REX_HOOK_RAW(sub_821FF3C8){
 if(pdz_death_voice::Observe(ctx,base,pdz_death_voice::CuePath::OneShot)){ctx.r3.u64=0;return;}
 __imp__sub_821FF3C8(ctx,base);
}
