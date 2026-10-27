#pragma once
#include "mp_progression_policy.h"
#include <algorithm>
#include <cmath>
namespace pdz_progression {
struct XpFrame {unsigned level;float fraction;bool complete;};
inline XpFrame AnimateXp(const Rank& before,const Rank& after,double seconds) {
  const unsigned crossed=after.level>=before.level?after.level-before.level:0;
  const unsigned segment=std::min(crossed,unsigned(std::max(0.,seconds)/1.1));
  const unsigned level=before.level+segment;
  const float start=segment?0.f:float(before.level_xp)/float(RequiredXp(level));
  const bool capped=level==(after.master?1000u:50u);
  const float end=segment<crossed||capped?1.f:float(after.level_xp)/float(RequiredXp(level));
  const float t=float(std::clamp((seconds-segment*1.1)/1.1,0.,1.));
  return {level,std::clamp(start+(end-start)*(t*t*(3-2*t)),0.f,1.f),seconds>=(crossed+1)*1.1};
}
}
