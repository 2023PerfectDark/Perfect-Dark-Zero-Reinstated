#pragma once
#include "pd_menu_state.h"
#include "mission_summary_state.h"
#include "cheat_unlock_audio.h"
#include "input_settings.h"
#include "mission_weather.h"
#include "pd_menu_classic.h"
#include "mp_progression_menu.h"
#include "weapon_challenge.h"
#include "cheat_progress.h"
#include "perfect_dark_zero_mode.h"
#include "pd_menu_help.h"
#include "update_service.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <rex/system/achievement_manager.h>
#include <rex/system/kernel_state.h>
#include <rex/runtime.h>

namespace pdz_menu {
// Palette adapted from n64decomp/perfect_dark src/game/menu.c g_MenuColours
// (169ed48bdcbfb3b568b028bd5bebb27680073514). Copyright 2022 Ryan Dwyer,
// MIT; see mod_assets/licenses/perfect-dark-menu-MIT.txt. Rendering is host
// ImGui rather than N64 display lists; no original game textures are included.
inline ImVec4 Colour(uint32_t rgba) {
  return {float(rgba>>24)/255, float((rgba>>16)&255)/255,
          float((rgba>>8)&255)/255, float(rgba&255)/255};
}
struct Palette { uint32_t border, title, edge, heading, body, text; };
inline constexpr Palette blue{0x0060bf7f,0x0000507f,0x00f0ff7f,0xffffffff,0x00002f7f,0x00ffffff};
inline constexpr Palette green{0x00bf007f,0x0050007f,0x00ff007f,0xffff00ff,0x002f007f,0x55ff55ff};

// Shared green notification style for future cheat/rank unlock callers.
inline std::mutex notice_mutex;
inline std::string notice_title, notice_body;
inline double notice_until=0;
inline void Notify(std::string title, std::string message) {
  std::lock_guard lock(notice_mutex);
  notice_title=std::move(title); notice_body=std::move(message); notice_until=-1;
}
class OverlayDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit OverlayDialog(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate) : ImGuiDialog(drawer), snow_texture_(pdz_weather::LoadSnowTexture(immediate)) {}
 private:
  std::unique_ptr<rex::ui::ImmediateTexture> snow_texture_;
  Classic classic_;
  int tab_=0,previous_tab_=-1;
  int cheat_tab_=0;
  static constexpr const char* cheat_tabs_[3]={"Cheats","Challenges","Unlocks"};
  static constexpr const char* tabs_[7]={"Controls","Graphics","Gameplay","Cheats","Achievements","Interface","Display"};
  bool saved_=false, was_open_=false, had_gamepad_=false;
  unsigned previous_=Closed;
  ImGuiConfigFlags old_nav_=0;
  std::string notes_;
  void DrawResults(const ImGuiViewport* vp,float scale) {
    const auto s=pdz_results::Snapshot();
    ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+vp->Size.y*.5f},0,{.5f,.5f});
    ImGui::SetNextWindowSize({270*scale,0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,Colour(0x002000ee));
    ImGui::PushStyleColor(ImGuiCol_Border,Colour(green.edge));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,scale});
    ImGui::Begin("##PDMissionSummary",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
    const auto tint=ImGui::ColorConvertFloat4ToU32(Colour(green.text));
    classic_.Text("PDZ Mission "+std::to_string(s.mission)+": Completed",ImGui::ColorConvertFloat4ToU32(Colour(green.heading)));
    ImGui::Separator();
    auto row=[&](const char* label,const std::string& value) {
      const auto p=ImGui::GetCursorScreenPos();
      const float width=ImGui::GetContentRegionAvail().x;
      auto* draw=ImGui::GetWindowDrawList();
      classic_.font.Text(draw,p,scale,tint,label);
      const float w=classic_.font.Text(nullptr,{},scale,tint,value);
      classic_.font.Text(draw,{p.x+width-w,p.y},scale,tint,value);
      ImGui::Dummy({width,13*scale});
    };
    row("Mission Status:","Completed");
    row("Best Time:",s.best_cs?pdz_results::Clock(s.best_cs):"--");
    row("Target Time:",s.target_seconds?pdz_results::Clock(uint64_t(s.target_seconds)*100):"--");
    row("Mission Time:",pdz_results::Clock(s.time_cs));
    constexpr const char* difficulty[]{"Agent","Secret Agent","Perfect Agent","Dark Agent"};
    row("Difficulty:",difficulty[std::min(s.difficulty,3u)]);
    ImGui::Separator();
    if(s.newly_unlocked)classic_.Text("New Cheat Available: Alerted Guards",tint);
    row("Kills:",std::to_string(s.kills));
    char accuracy[32];std::snprintf(accuracy,sizeof(accuracy),"%.1f%%",pdz_results::Accuracy(s));
    row("Accuracy:",accuracy);
    row("Shot Total:",std::to_string(s.shots));
    row("  Head Shots:",std::to_string(s.head_hits));
    row("  Body Shots:",std::to_string(s.body_hits));
    row("  Limb Shots:",std::to_string(s.limb_hits));
    ImGui::Separator();
    classic_.Text("Press START / Enter",tint);
    ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor(2);
    if(pdz_results::dismiss.exchange(false)||ImGui::IsKeyPressed(ImGuiKey_Enter,false)) {
      back=false;page=Closed;
    }
  }
  void ReadNotes() {
    notes_.clear();
    for (const auto& part : pdz_update::ReadChangelog()) {
      // UTF-16 release notes to UTF-8, preserving non-English text.
      for(size_t i=0;i<part.size();++i) {
        uint32_t c=part[i];
        if(c>=0xd800&&c<=0xdbff&&i+1<part.size()&&part[i+1]>=0xdc00&&part[i+1]<=0xdfff)
          c=0x10000+((c-0xd800)<<10)+(part[++i]-0xdc00);
        if(c<128)notes_+=char(c);
        else if(c<2048){notes_+=char(0xc0|(c>>6));notes_+=char(0x80|(c&63));}
        else if(c<65536){notes_+=char(0xe0|(c>>12));notes_+=char(0x80|((c>>6)&63));notes_+=char(0x80|(c&63));}
        else{notes_+=char(0xf0|(c>>18));notes_+=char(0x80|((c>>12)&63));notes_+=char(0x80|((c>>6)&63));notes_+=char(0x80|(c&63));}
      }
      notes_+=' ';
    }
  }
  void OnDraw(ImGuiIO& io) override {
    pdz_weather::Draw(snow_texture_.get());
    const auto* vp=ImGui::GetMainViewport();
    // Original 320x240 menu coordinates, rasterized at the current viewport
    // resolution (4.5x at 1080p, 9x at 2160p). Never stretch the font aspect.
    const bool extended=page.load()==Extended;
    const float option_width=300.f*float(std::clamp(REXCVAR_GET(pdz_ui_width),.85,1.4));
    const float logical_width=extended?option_width+146.f:340.f;
    const float requested=std::min(vp->Size.y/340.f,vp->Size.x/500.f)*float(std::clamp(REXCVAR_GET(pdz_ui_scale),.65,1.4));
    const float scale=std::min({requested,vp->Size.y/286.f,vp->Size.x/(logical_width+36.f)});
    classic_.scale=scale;
    {
      std::lock_guard lock(notice_mutex);
      if(notice_until>=0&&notice_until<=ImGui::GetTime()) {
        pdz_progression::Notice next;
        if(pdz_progression::career.NextNotice(next)) {
          notice_title="ACHIEVEMENT UNLOCKED";notice_body=next.name+"  +"+std::to_string(next.xp)+" XP";notice_until=-1;
          pdz_cheats::unlock_audio.Play();
        }
      }
      if(notice_until<0)notice_until=ImGui::GetTime()+6;
      if(notice_until>ImGui::GetTime()) {
        ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+12*scale},0,{.5f,0});
        ImGui::SetNextWindowSize({240*scale,0});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,Colour(green.body));
        ImGui::PushStyleColor(ImGuiCol_Border,Colour(green.edge));
        ImGui::Begin("##PDNotification",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs);
        classic_.Text(notice_title,ImGui::ColorConvertFloat4ToU32(Colour(green.heading)));
        ImGui::Separator(); classic_.Wrapped(notice_body,ImGui::ColorConvertFloat4ToU32(Colour(green.text)));
        ImGui::End(); ImGui::PopStyleColor(2);
      }
    }
    const unsigned current=page.load();
    if(!Overlay()) {
      if(was_open_) {
        io.ConfigFlags=(io.ConfigFlags & ~(ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad))|old_nav_;
        io.BackendFlags=(io.BackendFlags & ~ImGuiBackendFlags_HasGamepad)|(had_gamepad_?ImGuiBackendFlags_HasGamepad:0);
        for(auto key:{ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadFaceRight,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadDown,ImGuiKey_GamepadDpadLeft,ImGuiKey_GamepadDpadRight})io.AddKeyEvent(key,false);
      }
      was_open_=false;previous_=Closed;
      std::lock_guard lock(input_mutex);keys.clear();return;
    }
    if(!was_open_)had_gamepad_=(io.BackendFlags&ImGuiBackendFlags_HasGamepad)!=0;
    if(!was_open_)old_nav_=io.ConfigFlags & (ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad);
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    {
      std::lock_guard lock(input_mutex);
      for(const auto& k:keys) {
        ImGuiKey target=ImGuiKey_None;
        switch(k.code){case 0x5800:target=ImGuiKey_GamepadFaceDown;break;
          case 0x5814:if(current==Results&&(k.flags&1))pdz_results::dismiss=true;break;
          case 0x5801:if(k.flags&1)back=true;break;
          case 0x5804:if(k.flags&1){if(current==Extended)tab_=(tab_+6)%7;else if(current==Progression)progression_tab=(progression_tab+2)%3;}break;
          case 0x5805:if(k.flags&1){if(current==Extended)tab_=(tab_+1)%7;else if(current==Progression)progression_tab=(progression_tab+1)%3;}break;
          case 0x5810:case 0x5820:target=ImGuiKey_GamepadDpadUp;break;
          case 0x5811:case 0x5821:target=ImGuiKey_GamepadDpadDown;break;
          case 0x5812:case 0x5822:target=ImGuiKey_GamepadDpadLeft;break;
          case 0x5813:case 0x5823:target=ImGuiKey_GamepadDpadRight;break;}
        if(target!=ImGuiKey_None)io.AddKeyEvent(target,(k.flags&2)==0);
      }
      keys.clear();
    }
    if(current==Results) {
      if(previous_!=Results && pdz_results::Snapshot().newly_unlocked)pdz_cheats::unlock_audio.Play();
      was_open_=true;previous_=Results;
      DrawResults(vp,scale);return;
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)||ImGui::IsMouseClicked(1))back=true;
    bool changed=previous_!=current||(current==Extended&&previous_tab_!=tab_);
    if(current==Changelog&&changed)ReadNotes();
    ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+vp->Size.y*.5f},0,{.5f,.5f});
    const float panel_width=(current==Progression||current==Cheats)?340.f:current==Changelog?320.f:current==Extended?logical_width:240.f;
    const float panel_height=(current==Extended||current==Progression||current==Cheats)?252.f:current==Changelog?220.f:158.f;
    ImGui::SetNextWindowSize({panel_width*scale,panel_height*scale});
    if(changed)ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{2*scale,3*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,1*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,Colour(blue.body));
    ImGui::PushStyleColor(ImGuiCol_Border,Colour(blue.edge));
    ImGui::PushStyleColor(ImGuiCol_Text,Colour(blue.text));
    ImGui::PushStyleColor(ImGuiCol_Button,Colour(0x00306070));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Colour(0x008fafa0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,Colour(0x00afcfc0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,Colour(0x00306070));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,Colour(blue.text));
    ImGui::PushStyleColor(ImGuiCol_Separator,Colour(blue.border));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight,ImVec4(0,0,0,0));
    ImGui::Begin("##PerfectDarkCustomMenu",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.0f);
    auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    auto* panel=ImGui::GetWindowDrawList();
    // Side-page labels and arrows only where a real sibling page exists.
    if(current==Changelog||current==Updates) {
      const bool previous=current==Changelog;
      const float x=previous?pos.x-6*scale:pos.x+size.x+6*scale;
      const float y=pos.y+size.y*.5f;
      auto* tabs=ImGui::GetForegroundDrawList();
      const bool hover=ImGui::IsMouseHoveringRect({x-6*scale,y-45*scale},{x+6*scale,y+45*scale},false);
      const ImU32 tint=hover?IM_COL32_WHITE:IM_COL32(190,255,255,220);
      tabs->AddTriangleFilled({x+(previous?-4:4)*scale,y},{x+(previous?3:-3)*scale,y-4*scale},{x+(previous?3:-3)*scale,y+4*scale},tint);
      const char* caption=previous?"Updates":"Changelogs";
      const int begin=tabs->VtxBuffer.Size;
      classic_.font.Text(tabs,{x,y+7*scale},scale*.7f,tint,caption);
      for(int i=begin;i<tabs->VtxBuffer.Size;++i){
        auto& point=tabs->VtxBuffer[i].pos;
        float dx=point.x-x,dy=point.y-(y+7*scale);
        point={x-dy,y+7*scale+dx};
      }
      if(hover&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){page=previous?Updates:Changelog;}
      if(!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&
         (ImGui::IsKeyPressed(previous?ImGuiKey_LeftArrow:ImGuiKey_RightArrow)||
          ImGui::IsKeyPressed(previous?ImGuiKey_GamepadDpadLeft:ImGuiKey_GamepadDpadRight)))
        {page=previous?Updates:Changelog;}
    }
    if(current==Extended||current==Cheats){
      const int count=current==Cheats?3:7;
      int& active_tab=current==Cheats?cheat_tab_:tab_;
      for(int direction:{-1,1}){
        const int next=(active_tab+direction+count)%count;
        const float x=direction<0?pos.x-6*scale:pos.x+size.x+6*scale;
        const float y=pos.y+size.y*.5f;
        auto* d=ImGui::GetForegroundDrawList();
        const bool hover=ImGui::IsMouseHoveringRect({x-6*scale,y-45*scale},{x+6*scale,y+60*scale},false);
        const ImU32 tint=hover?IM_COL32_WHITE:IM_COL32(190,255,255,220);
        d->AddTriangleFilled({x+direction*4*scale,y},{x-direction*3*scale,y-4*scale},{x-direction*3*scale,y+4*scale},tint);
        const int begin=d->VtxBuffer.Size;
        classic_.font.Text(d,{x,y+7*scale},scale*.7f,tint,current==Cheats?cheat_tabs_[next]:tabs_[next]);
        for(int i=begin;i<d->VtxBuffer.Size;++i){auto& p=d->VtxBuffer[i].pos;float dx=p.x-x,dy=p.y-y-7*scale;p={x-dy,y+7*scale+dx};}
        if(hover&&ImGui::IsMouseClicked(0)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)){active_tab=next;saved_=false;}
      }
      // Shoulder buttons/Page Up/Down change tabs; left/right remain slider controls.
      if(!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)){
        if(ImGui::IsKeyPressed(ImGuiKey_PageUp)||ImGui::IsKeyPressed(ImGuiKey_GamepadL1))active_tab=(active_tab+count-1)%count;
        if(ImGui::IsKeyPressed(ImGuiKey_PageDown)||ImGui::IsKeyPressed(ImGuiKey_GamepadR1))active_tab=(active_tab+1)%count;
      }
    }
    if(current==Extended&&previous_tab_!=tab_)changed=true;
    // Subtle translucent blue light bands, as in the reference's glass panel.
    panel->AddRectFilledMultiColor(pos,{pos.x+size.x,pos.y+size.y},IM_COL32(0,12,65,80),IM_COL32(0,8,45,35),IM_COL32(0,0,25,80),IM_COL32(0,20,65,45));
    // menugfx_render_dialog_background: one logical pixel on three edges.
    panel->AddRectFilled({pos.x,pos.y},{pos.x+scale,pos.y+size.y},ImGui::ColorConvertFloat4ToU32(Colour(blue.border)));
    panel->AddRectFilled({pos.x+size.x-scale,pos.y},{pos.x+size.x,pos.y+size.y},ImGui::ColorConvertFloat4ToU32(Colour(blue.edge)));
    panel->AddRectFilledMultiColor({pos.x,pos.y+size.y-scale},{pos.x+size.x,pos.y+size.y},ImGui::ColorConvertFloat4ToU32(Colour(blue.border)),ImGui::ColorConvertFloat4ToU32(Colour(blue.edge)),ImGui::ColorConvertFloat4ToU32(Colour(blue.edge)),ImGui::ColorConvertFloat4ToU32(Colour(blue.border)));
    panel->AddRectFilledMultiColor(pos,{pos.x+size.x,pos.y+6*scale},IM_COL32(0,35,110,220),IM_COL32(0,35,110,220),IM_COL32(0,0,30,220),IM_COL32(0,0,30,220));
    panel->AddRectFilledMultiColor({pos.x,pos.y+6*scale},{pos.x+size.x,pos.y+12*scale},IM_COL32(0,0,30,220),IM_COL32(0,0,30,220),IM_COL32(0,195,205,210),IM_COL32(0,100,160,210));
    classic_.Text(current==Cheats?cheat_tabs_[cheat_tab_]:current==Progression?"MP Progression":current==Extended?tabs_[tab_]:current==Changelog?"Changelogs":"Check for Updates",IM_COL32_WHITE);
    ImGui::Separator();
    if(changed)classic_.selected_help.clear();
    if(current==Progression)DrawProgressionTabs(classic_);
    // Separate child identities retain each page's scroll position, including
    // when returning from another tab or closing and reopening Progression.
    ImGui::PushID(int(current));
    ImGui::PushID(current==Cheats?cheat_tab_:current==Progression?progression_tab:tab_);
    ImGui::BeginChild("scrolling-options",{current==Extended?option_width*scale:0,-16*scale},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(1.0f);
    if(current==Cheats&&cheat_tab_==2){
      classic_.Section("Alerted Guards");
      classic_.Wrapped("Complete Nightclub on Modern Agent in under 4:00.");
      classic_.Text(pdz_cheats::progress.Unlocked()?"Unlocked":"Locked");
      classic_.Section("Perfect Dark Zero");
      classic_.Wrapped("Complete every campaign mission on Dark Agent, with any campaign rules.");
      classic_.Text(pdz_custom::Unlocked()?"Unlocked":"Locked");
      classic_.Section("DK Mode");
      classic_.Wrapped("Miss three headshots while aiming at maximum zoom.");
      classic_.Section("Invisibility");
      classic_.Wrapped("Complete the campaign once.");
      classic_.Section("Invincibility");
      classic_.Wrapped("Complete the campaign once.");
    }else if(current==Cheats&&cheat_tab_==0){
      classic_.Settings(saved_,3,false);
    }else if(current==Cheats){
      if(changed)ImGui::SetKeyboardFocusHere();
      classic_.Section("Perfect Dark Zero");
      if(pdz_custom::Unlocked()) {
        bool enabled=REXCVAR_GET(pdz_custom_mode);
        if(classic_.Toggle("Custom mission mode",enabled)){REXCVAR_SET(pdz_custom_mode,enabled);saved_=false;}
        classic_.Wrapped("Enemy settings (%). Health, damage and accuracy retain native values at 100. Reaction speed 100 adds a one-second firing delay; higher values shorten it.");
#define PD_ENEMY(label,name) {float v=float(REXCVAR_GET(name));if(classic_.Number(label,v,10.f,1000.f,10.f)){REXCVAR_SET(name,double(v));saved_=false;}}
        PD_ENEMY("Enemy health",pdz_enemy_health);
        PD_ENEMY("Enemy damage",pdz_enemy_damage);
        PD_ENEMY("Enemy accuracy",pdz_enemy_accuracy);
        PD_ENEMY("Reaction speed",pdz_enemy_reaction);
#undef PD_ENEMY
        if(classic_.Row("Save challenge settings")){rex::cvar::SaveConfig(utils::SettingsDir()/"hardware.toml");saved_=true;}
      }else classic_.Wrapped("Locked: complete the campaign on Dark Agent.");
      classic_.Section("Mission unlocks");
      const bool guards_unlocked=pdz_cheats::progress.Unlocked();
      classic_.Text(guards_unlocked?"Alerted Guards - Unlocked":"Alerted Guards - Locked",
          guards_unlocked?IM_COL32(90,255,120,255):IM_COL32(175,85,85,255));
      if(guards_unlocked)
        classic_.Wrapped("Unlock earned. Guard-wave gameplay is not available in this build.");
      else
        classic_.Wrapped("Complete Nightclub on Modern Agent in under 4:00 to unlock.");
      const auto unlock_error=pdz_cheats::progress.Error();
      if(!unlock_error.empty())classic_.Wrapped(unlock_error,IM_COL32(255,170,70,255));
      classic_.Section("Weapon-only challenges");
      classic_.Wrapped("Select a weapon, then start a mission from the normal mission menu.");
      if(classic_.Row("Disable challenge",nullptr,pdz_challenge::Selected()==0))challenge_request=0;
      for(const auto& weapon:pdz_challenge::weapons){
        const std::string name(weapon.name.begin(),weapon.name.end());
        if(classic_.Row(name.c_str(),nullptr,pdz_challenge::Selected()==weapon.id))challenge_request=int(weapon.id);
      }
    }else if(current==Progression){
      DrawProgression(classic_);
    }else if(current==Extended){
      if(changed)ImGui::SetKeyboardFocusHere();
      if(tab_!=4)classic_.Settings(saved_,tab_);
      else {
        // Dialogs are created before the title runtime. Resolve this only when
        // the achievement tab is drawn, including after a profile/runtime change.
        auto* runtime=rex::Runtime::instance();
        auto* kernel=runtime?runtime->kernel_state():nullptr;
        auto* manager=kernel?&kernel->achievements():nullptr;
        const auto catalog=manager?manager->ListAchievements():std::vector<rex::system::AchievementInfo>{};
        unsigned count=0,score=0;
        for(const auto& a:catalog)if(manager->IsUnlocked(a.id)){++count;score+=a.gamerscore;}
        classic_.Text(std::to_string(count)+" / "+std::to_string(catalog.size())+" unlocked - "+std::to_string(score)+" G");
        ImGui::Separator();
        if(catalog.empty())classic_.Wrapped("Achievement information is not available yet.");
        for(const auto& a:catalog){
          ImGui::PushID(int(a.id));
          const bool unlocked=manager->IsUnlocked(a.id);
          const ImU32 status=unlocked?IM_COL32(90,255,120,255):IM_COL32(175,85,85,255);
          classic_.Wrapped(a.label,status);
          classic_.Text(std::string(unlocked?"Unlocked - ":"Locked - ")+std::to_string(a.gamerscore)+" G",status);
          classic_.Wrapped(unlocked?a.description:a.unachieved_description,unlocked?IM_COL32(130,210,150,255):IM_COL32(130,130,135,255));
          ImGui::Separator();ImGui::PopID();
        }
      }
    }
    else if(current==Changelog){
      if(classic_.Row("Reload changelog"))ReadNotes();
      ImGui::Separator(); classic_.Wrapped(notes_);
    }else{
      const auto state=pdz_update::Poll();
      classic_.Wrapped(state.message);
      if(state.phase=="downloading"||state.phase=="verifying")ImGui::ProgressBar(state.percent/100.f);
      ImGui::Separator();
      if(changed)ImGui::SetKeyboardFocusHere();
      const char* action=state.phase=="ready"?"Restart now":state.phase=="available"?"Download update":"Check again";
      if(classic_.Row(action)){
        if(state.phase=="ready")pdz_update::Restart();else if(state.phase=="available")pdz_update::Download();else pdz_update::Check();
      }
      if(classic_.Row(state.phase=="ready"?"Wait / restart later":"Redownload")){
        if(state.phase=="ready")pdz_update::Wait();else pdz_update::Redownload();
      }
      if(classic_.Row("Changelogs")){pdz_update::Wait();page=Changelog;}
      if(state.seconds>=0)ImGui::Text("Restarting in %d seconds",state.seconds);
    }
    const float scroll=ImGui::GetScrollY(),max_scroll=ImGui::GetScrollMaxY();
    const auto clip_pos=ImGui::GetWindowPos(),clip_size=ImGui::GetWindowSize();
    auto* scroll_draw=ImGui::GetWindowDrawList();
    float arrow_x=clip_pos.x+clip_size.x-5*scale;
    if(scroll>1)scroll_draw->AddTriangleFilled({arrow_x-3*scale,clip_pos.y+4*scale},{arrow_x+3*scale,clip_pos.y+4*scale},{arrow_x,clip_pos.y+scale},IM_COL32(190,255,255,200));
    if(scroll<max_scroll-1)scroll_draw->AddTriangleFilled({arrow_x-3*scale,clip_pos.y+clip_size.y-4*scale},{arrow_x+3*scale,clip_pos.y+clip_size.y-4*scale},{arrow_x,clip_pos.y+clip_size.y-scale},IM_COL32(190,255,255,200));
    ImGui::EndChild();
    ImGui::PopID();ImGui::PopID();
    if(current==Extended){
      ImGui::SameLine(0,6*scale);
      ImGui::BeginChild("option-help",{0,-16*scale},true,ImGuiWindowFlags_NoScrollbar);
      classic_.Text("Help",IM_COL32(255,210,80,255));
      ImGui::Separator();
      classic_.Wrapped(tab_==4?"Green achievements are unlocked. Muted red achievements are locked; their requirements appear in gray.":HelpFor(classic_.selected_help));
      if(tab_!=4){
        ImGui::Separator();
        classic_.Wrapped(ImpactFor(classic_.selected_help),IM_COL32(255,180,80,255));
      }
      ImGui::Dummy({0,8*scale});
      // Stable framed slot for future per-option screenshot comparisons.
      const auto p=ImGui::GetCursorScreenPos();
      const float w=ImGui::GetContentRegionAvail().x;
      ImGui::GetWindowDrawList()->AddRect(p,{p.x+w,p.y+w*9/16},IM_COL32(65,130,160,180));
      ImGui::Dummy({w,w*9/16});
      classic_.Wrapped("Picture preview",IM_COL32(135,155,170,255));
      if(classic_.selected_help=="Sniper headshot gore"||classic_.selected_help=="First-person body")
        classic_.Wrapped("Off / On comparison",IM_COL32(135,155,170,255));
      if(tab_==5)classic_.Wrapped("Menu and subtitle previews",IM_COL32(135,155,170,255));
      ImGui::EndChild();
    }
    ImGui::Separator();
    if(classic_.Row("Back"))back=true;
    ImGui::End();ImGui::PopStyleColor(10);ImGui::PopStyleVar(5);
    was_open_=true;previous_=current;previous_tab_=tab_;
  }
};
}



