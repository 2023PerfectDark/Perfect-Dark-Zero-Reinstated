#pragma once
#include <algorithm>
#include <cmath>
namespace landing_motion {
// Actor-position based, independent of render rate and camera bob. Units are cm.
struct State {
 bool valid=false;
 float previousY=0,drop=0,fallTime=0,peakSpeed=0,settled=0;
 float age=10,depth=0,duration=.6f;
 void Reset(){*this=State{};}
 float Step(float dt,float y,bool active,bool discontinuity=false){
  if(!active||discontinuity||!std::isfinite(y)||!std::isfinite(dt)||dt<=0||dt>.1f){Reset();return 0;}
  if(!valid){valid=true;previousY=y;return 0;}
  const float dy=y-previousY;previousY=y;
  if(std::abs(dy)>150.f){Reset();return 0;} // teleport/checkpoint, not a landing
  const float down=-dy/dt;
  age+=dt;
  if(down>25.f){
   drop-=dy;fallTime+=dt;peakSpeed=std::max(peakSpeed,down);settled=0;
  }else if(drop>0){
   settled+=dt;
   if(dy>3.f||settled>=.035f){
    // Reject stairs, ordinary gait and small changes in floor height.
    if(dy<=3.f&&drop>=55.f&&fallTime>=.12f&&peakSpeed>=140.f){
     depth=std::clamp(7.f+std::sqrt(drop-55.f)*1.25f+std::max(0.f,drop-100.f)*.07f,7.f,52.f);
     duration=.48f+depth*.008f;age=0;
    }
    drop=fallTime=peakSpeed=settled=0;
   }
  }
  if(age>=duration)return 0;
  constexpr float bend=.10f;
  const float t=std::clamp(age<bend?age/bend:(age-bend)/(duration-bend),0.f,1.f);
  const float smooth=t*t*(3.f-2.f*t);
  return depth*(age<bend?smooth:1.f-smooth);
 }
};
}
