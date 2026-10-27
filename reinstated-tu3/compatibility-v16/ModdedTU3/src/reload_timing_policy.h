#pragma once
#include <algorithm>
#include <cmath>
namespace pdz_reload {
// NTSC Perfect Dark animation frame counts, played at 60 frames/second.
// Source: n64decomp/perfect_dark src/assets/ntsc-final/animations.json and
// src/game/invitems.c. PDZ-only weapons use their closest weapon family.
inline unsigned ClassicFrames(unsigned weapon) {
  switch(weapon){
    case 2:case 7:case 15:return 92; // P9P/Falcon/MagSec
    case 11:return 166; // DY357
    case 16:return 98; // Psychosis: tranquilizer
    case 3:case 17:case 39:return 128; // DW-P5/CMP/Klobb
    case 5:return 144; // UGL: Cyclone
    case 1:case 9:return 134; // KSI/FAC: AR34
    case 13:return 108; // SuperDragon
    case 8:return 124; // Laptop
    case 10:return 208; // M60: Reaper
    case 18:return 138; // Plasma: Callisto
    case 20:return 154; // RCP-90: RCP120
    case 12:return 130; // DEF-12: Shotgun (full reload)
    case 23:return 140; // Mayhem: Rocket launcher
    case 37:return 158; // Jackal: Sniper
    case 38:return 130; // Shockwave: Farsight
    default:return 0;
  }
}
inline double ModernSeconds(unsigned weapon,bool empty) {
  double seconds=0;
  switch(weapon){
    case 2:case 7:case 15:case 16:seconds=1.8;break;
    case 11:seconds=2.5;break;
    case 3:case 5:case 17:case 39:seconds=2.1;break;
    case 1:case 9:case 13:case 8:case 18:case 20:seconds=2.3;break;
    case 10:seconds=4.0;break;
    case 12:seconds=2.4;break;
    case 23:seconds=3.0;break;
    case 37:case 38:seconds=2.8;break;
  }
  return seconds?seconds+(empty?.3:0.):0.;
}
inline double Speed(unsigned rules,unsigned weapon,bool empty,double duration,double native) {
  if(!std::isfinite(duration)||!std::isfinite(native)||duration<=0||native<=0)return native;
  double target=rules==1?ModernSeconds(weapon,empty):rules==2&&ClassicFrames(weapon)?(ClassicFrames(weapon)-1)/60.:0.;
  if(target<=0)return native;
  // Bound abnormal/custom assets; never accelerate unrelated animations.
  const double desired=rules==1?std::max(native*1.15,duration/target):duration/target;
  return std::clamp(desired,native*.25,native*4.);
}
}
