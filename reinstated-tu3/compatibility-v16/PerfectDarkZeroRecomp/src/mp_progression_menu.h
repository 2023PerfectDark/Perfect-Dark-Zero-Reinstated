#pragma once
#include "pd_menu_classic.h"
#include "mp_progression_runtime.h"
#include <ctime>
namespace pdz_menu {
inline int progression_tab=0;
inline void DrawProgressionTabs(Classic& ui) {
  constexpr const char* names[]={"Career","Achievements","Prestige"};
  const float width=ImGui::GetContentRegionAvail().x/3.f;
  for(int i=0;i<3;++i){
    if(i)ImGui::SameLine(0,0);
    ImGui::PushID(i);
    const auto p=ImGui::GetCursorScreenPos();
    if(ImGui::Selectable("##career-tab",progression_tab==i,0,{width,16*ui.scale}))progression_tab=i;
    ui.font.Text(ImGui::GetWindowDrawList(),{p.x+3*ui.scale,p.y+2*ui.scale},ui.scale,
      progression_tab==i?IM_COL32(255,210,80,255):IM_COL32(0,255,255,255),names[i]);
    ImGui::PopID();
  }
  if(!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)){
    if(ImGui::IsKeyPressed(ImGuiKey_PageUp)||ImGui::IsKeyPressed(ImGuiKey_GamepadL1))progression_tab=(progression_tab+2)%3;
    if(ImGui::IsKeyPressed(ImGuiKey_PageDown)||ImGui::IsKeyPressed(ImGuiKey_GamepadR1))progression_tab=(progression_tab+1)%3;
  }
  ImGui::Separator();
}
inline void DrawProgression(Classic& ui) {
  using namespace pdz_progression;
  static int filter=0;
  static bool confirmPrestige=false,confirmReset=false,resetAchievements=false;
  const auto c=career.Snapshot();
  const auto error=career.Error();if(!error.empty())ui.Wrapped(error,IM_COL32(255,150,80,255));
  if(progression_tab==0){
  ui.Section("Combat career");
  ui.Text((c.rank.master?"Master Prestige":"Prestige "+std::to_string(c.rank.prestige))+" - Level "+std::to_string(c.rank.level));
  const bool capped=c.rank.level==(c.rank.master?1000u:50u);
  ui.Text(capped?"Level cap reached":std::to_string(c.rank.level_xp)+" / "+std::to_string(RequiredXp(c.rank.level))+" XP");
  ImGui::ProgressBar(capped?1.f:float(double(c.rank.level_xp)/RequiredXp(c.rank.level)),{0,5*ui.scale},"");
  ui.Text("Lifetime XP: "+std::to_string(c.rank.lifetime_xp));
  ui.Text("Kills: "+std::to_string(c.kills)+"   Deaths: "+std::to_string(c.deaths));
  ui.Text("Career resets: "+std::to_string(c.career_resets));
  char ratio[48];std::snprintf(ratio,sizeof(ratio),"Kill / death ratio: %.2f",double(c.kills)/std::max(uint64_t(1),c.deaths));ui.Text(ratio);
  unsigned completed=0;uint64_t bonus=0;
  for(unsigned i=0;i<c.unlocked.size();++i)if(c.unlocked[i]){++completed;bonus+=Achievements()[i].xp;}
  ui.Text("Achievements: "+std::to_string(completed)+" / 39");
  ui.Text("Completed achievement rewards: "+std::to_string(bonus)+" XP");
  ui.Section("Match and objective records");
  constexpr const char* labels[]={"Deathmatch games","DarkOps games","Killcount kills","Team Killcount kills",
    "Flags captured","Players infected","Territories held","Rounds survived","Property destroyed (CR)",
    "Survival time (seconds)","Sniper kills","Headshot kills","Melee kills","Explosive kills"};
  static_assert(std::size(labels)==unsigned(Counter::Count));
  for(unsigned i=0;i<c.counters.size();++i)ui.Text(std::string(labels[i])+": "+std::to_string(c.counters[i]));
  ui.Section("Weapon kills");
  struct Weapon {unsigned id;const char* name;};
  constexpr Weapon weapons[]={{2,"P9P"},{7,"Falcon"},{11,"DY357"},{15,"Magsec"},{16,"Psychosis"},
    {3,"DW-P5"},{5,"UGL"},{17,"CMP150"},{20,"RCP90"},{9,"FAC-16"},{1,"KSI-74"},{13,"SuperDragon"},
    {8,"Laptop Gun"},{10,"M60"},{18,"Plasma Rifle"},{12,"DEF-12"},{23,"Mayhem"},{37,"Jackal"},
    {38,"Shockwave"},{26,"Combat Shield"},{27,"Viblade"},{33,"Frag Grenade"},{30,"Flashbang"},{34,"Mine"},{35,"Hawk"}};
  std::array<bool,64> named{};
  for(const auto& w:weapons){named[w.id]=true;ui.Text(std::string(w.name)+": "+std::to_string(c.weapon_kills[w.id]));}
  for(unsigned i=0;i<c.weapon_kills.size();++i)if(!named[i]&&c.weapon_kills[i])
    ui.Text("Other weapon "+std::to_string(i)+": "+std::to_string(c.weapon_kills[i]));
  return;
  }
  if(progression_tab==2){
  ui.Section("Prestige progression");
  ui.Text((c.rank.master?"Master Prestige":"Prestige "+std::to_string(c.rank.prestige))+" - Level "+std::to_string(c.rank.level));
  ui.Wrapped("Reach level 50 to enter the next prestige. Complete Prestige 5 to enter Master Prestige, levels 1 to 1,000.");
  if(CanPrestige(c.rank)) {
    if(ui.Row(c.rank.prestige==5?"Enter Master Prestige":"Enter next prestige"))confirmPrestige=true;
    if(confirmPrestige) {
      ui.Wrapped("Return to level 1. Achievements and lifetime statistics are kept.");
      if(ui.Row("Confirm prestige")){career.EnterPrestige();confirmPrestige=false;}
      if(ui.Row("Cancel prestige"))confirmPrestige=false;
    }
  }else {confirmPrestige=false;ui.Wrapped(c.rank.master?"Master Prestige unlocked.":"Next prestige unlocks at level 50.");}
  ui.Section("Reset combat career");
  if(CanResetCareer(c.rank)) {
    if(ui.Row("Reset combat career"))confirmReset=true;
    if(confirmReset) {
      ui.Wrapped("This resets your rank, combat totals and weapon statistics to a new career.");
      ui.Toggle("Reset MP achievements too",resetAchievements);
      ui.Wrapped(resetAchievements?"Achievement progress will be cleared. Earn their XP rewards again.":"Keep achievement progress and completion. Earned bonuses are not awarded again.");
      if(ui.Row("Confirm career reset")){career.ResetProfile(resetAchievements);confirmReset=false;}
      if(ui.Row("Cancel career reset"))confirmReset=false;
    }
  }else {confirmReset=false;ui.Wrapped("Career reset unlocks at Master Prestige level 50.");}
  return;
  }
  ui.Section("MP achievements");
  unsigned completed=0;for(auto date:c.unlocked)if(date)++completed;
  ui.Text("Achievements Completed: "+std::to_string(completed)+" / 39");
  ImGui::ProgressBar(completed/39.f,{0,5*ui.scale},"");
  constexpr const char* filters[]={"All","Completed","In Progress"};
  ui.Choice("Show",filter,filters,3);
  for(unsigned i=0;i<Achievements().size();++i) {
    const auto& a=Achievements()[i];const bool done=c.unlocked[i]!=0;
    if((filter==1&&!done)||(filter==2&&done))continue;
    const auto value=std::min(c.counters[unsigned(a.counter)],a.target);
    ui.Wrapped(a.name,done?IM_COL32(90,255,120,255):IM_COL32(170,200,210,255));
    ui.Text(std::to_string(value)+" / "+std::to_string(a.target)+" ("+std::to_string(value*100/a.target)+"%)  +"+std::to_string(a.xp)+" XP");
    if(done) {
      const auto time=static_cast<time_t>(c.unlocked[i]);std::tm date{};char text[64]{};
#ifdef _WIN32
      gmtime_s(&date,&time);
#else
      gmtime_r(&time,&date);
#endif
      std::strftime(text,sizeof(text),"Unlocked: %Y-%m-%d UTC",&date);ui.Text(text);
    }
    ImGui::Separator();
  }
}
}
