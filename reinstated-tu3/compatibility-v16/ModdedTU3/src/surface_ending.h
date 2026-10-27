#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace pdz_surface {
// World-space opening measured from Surface's ventilation roof mesh.
struct VentEntry {
  bool armed=false, entered=false;
  bool Update(const std::array<float,3>& p) {
    for(float v:p)if(!std::isfinite(v))return false;
    if(entered)return false;
    const bool roof=p[0]>12900 && p[0]<14530 && p[2]>-11780 && p[2]<-10150;
    if(roof && p[1]>815 && p[1]<1100)armed=true;
    if(!roof){armed=false;return false;}
    const bool opening=p[0]>13502.3f && p[0]<13931.0f &&
        p[2]>-11214.9f && p[2]<-10663.3f;
    if(armed && opening && p[1]<825 && p[1]>650){entered=true;return true;}
    return false;
  }
};
inline float EndingFade(double seconds) {
  const float t=std::clamp(float((seconds-1.2)/1.0),0.f,1.f);
  return t*t*(3-2*t);
}
}
