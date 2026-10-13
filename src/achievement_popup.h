#pragma once
#include <rex/ui/overlay/achievement_notification.h>
#include <rex/system/achievement_manager.h>
#include "utils.h"
#include <Windows.h>
#include <mmsystem.h>
#include <deque>
#include <set>
#include <mutex>
#include <chrono>
#include <cmath>
#include <fstream>
#include <cctype>
#include <rex/ui/immediate_drawer.h>

// Shared queue outlives any in-flight guest callback during shutdown.
class PdzAchievementPopup final : public rex::ui::AchievementNotificationDialog {
 struct Pending { rex::system::AchievementEvent event; };
 struct State {
  std::mutex mutex;
  std::deque<Pending> queue;
  std::set<std::pair<uint32_t,uint64_t>> seen;
  void Push(const rex::system::AchievementEvent& event) {
   std::lock_guard lock(mutex);
   if(seen.emplace(event.achievement.id,event.unlocked_at_filetime).second)queue.push_back({event});
  }
 };
 std::shared_ptr<State> state_=std::make_shared<State>();
 rex::system::AchievementManager& manager_;
 uint64_t listener_=0;
 std::chrono::steady_clock::time_point started_{};
 bool showing_=false;
 bool rare_=false;
 rex::ui::ImmediateDrawer* renderer_;
 std::unique_ptr<rex::ui::ImmediateTexture> xbox_,trophy_,trophyBase_,diamond_,gamerscore_;
 bool art_loaded_=false;
 std::unique_ptr<rex::ui::ImmediateTexture> LoadArt(const char* name) {
  std::ifstream input(utils::SettingsDir()/"achievement-art"/(std::string(name)+".rgba"),std::ios::binary);
  uint32_t size[2]{};input.read(reinterpret_cast<char*>(size),8);
  if(!input||!size[0]||!size[1]||size[0]>4096||size[1]>4096)return {};
  std::vector<uint8_t> pixels(size_t(size[0])*size[1]*4);
  input.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
  if(!input)return {};
  return renderer_->CreateTexture(size[0],size[1],rex::ui::ImmediateTextureFilter::kLinear,false,pixels.data());
 }
 std::chrono::steady_clock::time_point replay_checked_{};
 public:
 PdzAchievementPopup(rex::ui::ImGuiDrawer* drawer,rex::system::AchievementManager& manager,rex::ui::ImmediateDrawer* renderer)
  :AchievementNotificationDialog(drawer),manager_(manager),renderer_(renderer) {
  std::weak_ptr<State> weak=state_;
  listener_=manager_.RegisterUnlockCallback([weak](const auto& event){if(auto state=weak.lock())state->Push(event);});
 }
 ~PdzAchievementPopup() override {manager_.UnregisterCallback(listener_);mciSendStringW(L"close pdzAchievement",nullptr,0,nullptr);}
 void Push(const rex::system::AchievementEvent& event) override {state_->Push(event);}
 private:
 void OnDraw(ImGuiIO&) override {
  rex::system::AchievementEvent event;
  const auto now=std::chrono::steady_clock::now();
  // One-shot local preview request: replay presentation without unlocking anything.
  if(now-replay_checked_>=std::chrono::seconds(1)) {
   replay_checked_=now;
   // Explicit, one-shot local unlock requested by the player.
   const auto unlockRequest=utils::SettingsDir()/"achievement-unlock-name.txt";
   std::ifstream unlockInput(unlockRequest);
   std::string requested;
   if(unlockInput&&std::getline(unlockInput,requested)) {
    unlockInput.close();
    auto normalize=[](const std::string& value){std::string result;for(unsigned char c:value)if(std::isalnum(c))result+=char(std::tolower(c));return result;};
    const auto wanted=normalize(requested);
    for(const auto& info:manager_.ListAchievements()) {
     if(!wanted.empty()&&normalize(info.label)==wanted) {
      std::error_code error;
      if(std::filesystem::remove(unlockRequest,error)) {
       const auto result=manager_.UnlockAchievement(info.id,rex::system::AchievementNotification::kShow);
       if(result==rex::system::AchievementUnlockResult::kAlreadyUnlocked) {
        std::lock_guard lock(state_->mutex);
        state_->queue.push_back({{info,manager_.GetUnlockTime(info.id)}});
       }
       REXLOG_INFO("Requested achievement unlocked: {} ({} G)",info.label,info.gamerscore);
      }
      break;
     }
    }
   }
   const auto request=utils::SettingsDir()/"achievement-replay.txt";
   std::ifstream input(request);
   if(input) {
    std::vector<rex::system::AchievementEvent> replay;
    uint32_t id;
    while(input>>id) {
     if(manager_.IsUnlocked(id))
      if(auto info=manager_.FindAchievement(id))replay.push_back({*info,manager_.GetUnlockTime(id)});
    }
    input.close();
    std::error_code error;
    if(std::filesystem::remove(request,error)) {
     std::lock_guard lock(state_->mutex);
     for(const auto& item:replay)state_->queue.push_back({item});
    }
   }
  }
  float age=0;
  {
   std::lock_guard lock(state_->mutex);
   if(showing_) {
    age=std::chrono::duration<float>(now-started_).count();
    if(age>=7.5f){state_->queue.pop_front();showing_=false;}
   }
   if(state_->queue.empty())return;
   if(!showing_) {
    started_=now;age=0;showing_=true;
    // Offline rarity policy; explicit IDs may be supplied in settings.
    rare_=state_->queue.front().event.achievement.gamerscore>=25;
    std::ifstream rarity(utils::SettingsDir()/"achievement-rare-ids.txt");
    uint32_t rareId;
    while(rarity>>rareId)if(rareId==state_->queue.front().event.achievement.id)rare_=true;
    const auto sound=utils::SettingsDir()/(rare_?"achievement-rare.mp3":"achievement-regular.mp3");
    mciSendStringW(L"close pdzAchievement",nullptr,0,nullptr);
    const auto command=L"open \""+sound.wstring()+L"\" type mpegvideo alias pdzAchievement";
    if(mciSendStringW(command.c_str(),nullptr,0,nullptr)==0)
     mciSendStringW(L"play pdzAchievement from 0",nullptr,0,nullptr);
    else REXLOG_WARN("Achievement popup: could not open {}",sound.string());
   }
   event=state_->queue.front().event;
  }
  if(!art_loaded_) {
   art_loaded_=true;
   xbox_=LoadArt("xbox");trophy_=LoadArt("trophy");trophyBase_=LoadArt("trophy-no-handles");
   diamond_=LoadArt("rare");gamerscore_=LoadArt("gamerscore");
  }
  // Sample the accelerated presentation at 60 Hz, independent of game simulation rate.
  age=std::floor(age*60.f)/60.f*1.4f;
  const auto* vp=ImGui::GetMainViewport();
  const float scale=vp->Size.y/720.f;
  auto smooth=[](float v){v=std::clamp(v,0.f,1.f);return v*v*(3.f-2.f*v);};
  const float t=age/10.5f;
  const float open=smooth((t-.11f)/.13f)*(1.f-smooth((t-.85f)/.04f));
  const float visible=smooth(t/.04f)*(1.f-smooth((t-.96f)/.03f));
  const float h=75*scale,w=(75+280*open)*scale;
  const float x=vp->Pos.x+(vp->Size.x-w)*.5f,y=vp->Pos.y+vp->Size.y-100*scale;
  auto* d=ImGui::GetForegroundDrawList();
  const int alpha=int(255*visible);
  const ImU32 white=IM_COL32(255,255,255,alpha);
  d->AddRectFilled({x,y},{x+w,y+h},IM_COL32(57,150,12,alpha),h/2);
  const ImVec2 c{x+h/2,y+h/2};
  d->AddCircleFilled(c,h*.5f*visible,IM_COL32(66,174,14,alpha),64);
  // Original CodePen SVGs and 147-frame diamond animation, rasterized for the native renderer.
  auto icon=[&](rex::ui::ImmediateTexture* texture,float width,float height,float opacity=1.f){
   if(texture)d->AddImage(reinterpret_cast<ImTextureID>(texture),
    {c.x-width*scale/2,c.y-height*scale/2},{c.x+width*scale/2,c.y+height*scale/2},
    {0,0},{1,1},IM_COL32(255,255,255,int(alpha*opacity)));
  };
  const float xboxOpacity=(1-smooth((t-.19f)/.04f))+smooth((t-.90f)/.05f);
  if(rare_&&age>=2.f&&age<9.5f&&diamond_) {
   const float position=std::clamp((age-2.f)/7.4f*147,0.f,146.f);
   const int frame=int(position);
   const float blend=position-frame;
   auto diamondFrame=[&](int index,float opacity){
    const float u=(index%15)/15.f,v=(index/15)/10.f;
    d->AddImage(reinterpret_cast<ImTextureID>(diamond_.get()),{c.x-h/2,c.y-h/2},{c.x+h/2,c.y+h/2},
     {u,v},{u+1.f/15,v+.1f},IM_COL32(255,255,255,int(alpha*opacity*(1-smooth((age-9.f)/.5f)))));
   };
   diamondFrame(frame,1.f);
   if(frame<146)diamondFrame(frame+1,blend);
  }
  if(!rare_) {
   const float trophyOpacity=smooth((t-.20f)/.04f)*(1-smooth((t-.90f)/.05f));
   icon(trophyBase_.get(),37.5f,37.5f,trophyOpacity);
   icon(trophy_.get(),37.5f*std::abs(std::cos(age*6.2831853f/3.f)),37.5f,trophyOpacity);
  }
  icon(xbox_.get(),37.5f*visible,37.5f*visible,std::clamp(xboxOpacity,0.f,1.f));
  const float textFade=smooth((t-.20f)/.05f)*(1-smooth((t-.79f)/.05f));
  if(textFade>0&&open>.1f) {
   const float slide=(1-smooth((t-.20f)/.05f))*60-smooth((t-.79f)/.05f)*80;
   d->PushClipRect({x+80*scale,y},{x+w-12*scale,y+h},true);
   auto text=[&](const std::string& value,float py,float size){
    float extent=ImGui::GetFont()->CalcTextSizeA(size*scale,10000,0,value.c_str()).x;
    float font=size*scale*std::min(1.f,255*scale/std::max(1.f,extent));
    d->AddText(ImGui::GetFont(),font,{x+87*scale,y+(py+slide)*scale},IM_COL32(255,255,255,int(alpha*textFade)),value.c_str());
   };
   text(rare_?"Rare achievement unlocked":"Achievement unlocked",18,16);
   if(gamerscore_)d->AddImage(reinterpret_cast<ImTextureID>(gamerscore_.get()),
    {x+87*scale,y+(42+slide)*scale},{x+104*scale,y+(59+slide)*scale},{0,0},{1,1},
    IM_COL32(255,255,255,int(alpha*textFade)));
   const std::string detail=std::to_string(event.achievement.gamerscore)+" - "+event.achievement.label;
   const float extent=ImGui::GetFont()->CalcTextSizeA(17*scale,10000,0,detail.c_str()).x;
   const float font=17*scale*std::min(1.f,230*scale/std::max(1.f,extent));
   d->AddText(ImGui::GetFont(),font,{x+110*scale,y+(42+slide)*scale},
    IM_COL32(255,255,255,int(alpha*textFade)),detail.c_str());
   d->PopClipRect();
  }
 }
};
