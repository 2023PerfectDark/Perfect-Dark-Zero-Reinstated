#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace pdz_weapons {
// Hand-authored reference timing. All offsets return to zero at both ends.
struct InspectKey { float time, pitch, yaw, roll, x, y, z, pose; };
inline InspectKey InspectionMotion(unsigned kind, float t) {
  static constexpr std::array<InspectKey,8> mac{{
    {0,0,0,0,0,0,0,.999f}, {.13f,-12,-24,35,-5,3,-3,.999f},
    {.30f,-14,-28,40,-6,4,-3,.999f}, {.43f,-8,5,8,-4,4,-2,.999f},
    {.58f,-12,27,-35,-3,4,-2,.999f}, {.73f,-10,25,-32,-3,3,-2,.999f},
    {.86f,-5,-5,10,-2,2,-1,.999f}, {1,0,0,0,0,0,0,.999f}
  }};
  static constexpr std::array<InspectKey,8> lachmann{{
    {0,0,0,0,0,0,0,0}, {.12f,-4,0,2,-18,4,5,.16f},
    {.27f,-5,0,3,-30,6,8,.32f}, {.43f,-4,0,3,-30,6,8,.39f},
    {.57f,-4,0,2,-30,5,7,.48f}, {.73f,-3,0,2,-22.5f,3,5,.68f},
    {.87f,-1,0,1,-10.5f,1,2,.88f}, {1,0,0,0,0,0,0,.999f}
  }};
  const auto& keys=kind==2?lachmann:mac;
  t=std::clamp(t,0.f,1.f);
  for(size_t i=1;i<keys.size();++i) if(t<=keys[i].time) {
    const auto& a=keys[i-1]; const auto& b=keys[i];
    float u=(t-a.time)/(b.time-a.time); u=u*u*(3-2*u);
    auto lerp=[&](float x,float y){return x+(y-x)*u;};
    return {t,lerp(a.pitch,b.pitch),lerp(a.yaw,b.yaw),lerp(a.roll,b.roll),
      lerp(a.x,b.x),lerp(a.y,b.y),lerp(a.z,b.z),lerp(a.pose,b.pose)};
  }
  return keys.back();
}
}
