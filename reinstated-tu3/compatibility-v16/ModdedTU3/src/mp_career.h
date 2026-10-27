#pragma once
#include "mp_progression_policy.h"
#include <array>
#include <vector>
#include <string>
#include <algorithm>

namespace pdz_progression {
enum class Counter : unsigned {
  DeathmatchGames, DarkOpsGames, KillcountKills, TeamKillcountKills,
  Flags, Infections, Territories, Rounds, Destruction, SurvivalSeconds,
  SniperKills, Headshots, MeleeKills, ExplosiveKills, Count
};
struct Achievement { std::string name; Counter counter; uint64_t target; uint32_t xp; };
inline const std::vector<Achievement>& Achievements() {
  static const auto definitions=[] {
    std::vector<Achievement> a;
    auto add=[&](Counter c,const char* label,unsigned tiers=3) {
      constexpr unsigned thresholds[]={10,100,1000},rewards[]={300,900,2250};
      for(unsigned i=0;i<tiers;++i)a.push_back({std::to_string(thresholds[i])+" "+label,c,thresholds[i],rewards[i]});
    };
    add(Counter::DeathmatchGames,"Deathmatch Games"); add(Counter::DarkOpsGames,"DarkOps Games");
    add(Counter::KillcountKills,"Kills in Killcount"); add(Counter::TeamKillcountKills,"Kills in Team Killcount");
    add(Counter::Flags,"Flags Captured"); add(Counter::Infections,"Players Infected");
    add(Counter::Territories,"Territories Held",2); add(Counter::Rounds,"Rounds Survived");
    a.push_back({"10,000 CR Property Destroyed",Counter::Destruction,10000,900});
    a.push_back({"100,000 CR Property Destroyed",Counter::Destruction,100000,2250});
    a.push_back({"10 Minutes Survived",Counter::SurvivalSeconds,600,300});
    a.push_back({"100 Minutes Survived",Counter::SurvivalSeconds,6000,900});
    add(Counter::SniperKills,"Sniper Kills"); add(Counter::Headshots,"Headshot Kills");
    add(Counter::MeleeKills,"Melee Kills"); add(Counter::ExplosiveKills,"Explosive Kills");
    return a;
  }();
  return definitions;
}
struct Career {
  Rank rank;
  std::array<uint64_t,static_cast<unsigned>(Counter::Count)> counters{};
  std::array<uint64_t,39> unlocked{}; // Unix timestamps; zero means locked.
  uint64_t kills=0,deaths=0,career_resets=0,last_event=0;
  std::array<uint64_t,64> weapon_kills{};
};
struct Event {
  uint64_t sequence=0; // Monotonic per persistent profile, supplied by the validated bridge.
  uint64_t xp=0,kills=0,deaths=0;
  unsigned weapon=0;
  std::array<uint64_t,static_cast<unsigned>(Counter::Count)> counters{};
};
inline std::vector<unsigned> Apply(Career& c,const Event& e,uint64_t now) {
  std::vector<unsigned> unlocked;
  if(!Valid(c.rank)||!e.sequence||e.sequence<=c.last_event||!now)return unlocked;
  c.last_event=e.sequence;
  c.kills=SaturatingAdd(c.kills,e.kills); c.deaths=SaturatingAdd(c.deaths,e.deaths);
  if(e.weapon<c.weapon_kills.size())c.weapon_kills[e.weapon]=SaturatingAdd(c.weapon_kills[e.weapon],e.kills);
  for(unsigned i=0;i<c.counters.size();++i)c.counters[i]=SaturatingAdd(c.counters[i],e.counters[i]);
  AwardXp(c.rank,e.xp);
  const auto& definitions=Achievements();
  for(unsigned i=0;i<definitions.size();++i) {
    const auto& a=definitions[i];
    if(!c.unlocked[i]&&c.counters[static_cast<unsigned>(a.counter)]>=a.target) {
      c.unlocked[i]=now; AwardXp(c.rank,a.xp); unlocked.push_back(i);
    }
  }
  return unlocked;
}
inline bool Reset(Career& c,bool resetAchievements) {
  if(!CanResetCareer(c.rank))return false;
  Career fresh;
  fresh.career_resets=SaturatingAdd(c.career_resets,1);
  fresh.last_event=c.last_event; // A reset must not replay the previous event.
  if(!resetAchievements) {fresh.counters=c.counters;fresh.unlocked=c.unlocked;}
  c=fresh; return true;
}
}
