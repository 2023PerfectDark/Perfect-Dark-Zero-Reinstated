#pragma once
#include "mp_progression_runtime.h"
#include "mp_xp_animation.h"
#include "cheat_unlock_audio.h"
#include <imgui.h>
namespace pdz_progression {
class XpOverlay {
  MatchXp match_{};Rank level_{};
  double match_time_=-1,level_time_=-1;
 public:
  void Draw(const ImGuiViewport* vp,float scale) {
    const double now=ImGui::GetTime();
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing;
    if(level_time_<0&&career.NextLevel(level_)){level_time_=now;pdz_cheats::unlock_audio.Play();}
    if(level_time_>=0){
      const float age=float(now-level_time_);
      if(age>=3.2f)level_time_=-1;
      else {
        const float fade=std::min({1.f,age/.18f,(3.2f-age)/.6f});
        ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+vp->Size.y*.28f},0,{.5f,.5f});
        ImGui::SetNextWindowSize({230*scale,0});
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,fade);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,{.01f,.04f,.12f,.94f});
        ImGui::Begin("##mp-level-up",nullptr,flags);
        ImGui::SetWindowFontScale(scale*(1.f+.16f*std::exp(-age*5)));
        ImGui::TextColored({1.f,.72f,.12f,1.f},"LEVEL UP");
        ImGui::Text("%sLEVEL %u",level_.master?"MASTER ":"",level_.level);
        ImGui::Text("PRESTIGE %u",level_.prestige);
        auto p=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
        auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(p,{p.x+width,p.y+3*scale},IM_COL32(25,100,190,255));
        const float sweep=std::min(1.f,age/.65f)*width;
        d->AddRectFilled(p,{p.x+sweep,p.y+3*scale},IM_COL32(255,195,40,255));
        ImGui::Dummy({width,4*scale});ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar();
      }
    }
    if(match_time_<0&&career.NextMatch(match_))match_time_=now;
    if(match_time_<0)return;

    const double age=now-match_time_;const auto f=AnimateXp(match_.before,match_.after,age);
    const double duration=(match_.after.level-match_.before.level+1)*1.1+6.;
    if(age>duration){match_time_=-1;return;}
    ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x*.5f,vp->Pos.y+vp->Size.y*.83f},0,{.5f,.5f});
    ImGui::SetNextWindowSize({300*scale,0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{.01f,.035f,.10f,.95f});
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,{.1f,.65f,1.f,1.f});
    ImGui::Begin("##mp-match-xp",nullptr,flags);ImGui::SetWindowFontScale(scale);
    ImGui::Text("MATCH XP  +%llu",static_cast<unsigned long long>(match_.after.lifetime_xp-match_.before.lifetime_xp));
    ImGui::Text("LEVEL %u",f.level);
    ImGui::ProgressBar(f.fraction,{-1,12*scale},"");
    ImGui::Text("0");ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::CalcTextSize("1").x-ImGui::GetStyle().WindowPadding.x);ImGui::Text("1");
    if(f.level==(match_.after.master?1000u:50u))ImGui::Text("MAX LEVEL");
    else ImGui::Text("%llu / %llu XP",static_cast<unsigned long long>(std::llround(f.fraction*RequiredXp(f.level))),static_cast<unsigned long long>(RequiredXp(f.level)));
    ImGui::End();ImGui::PopStyleColor(2);
  }
};
}
