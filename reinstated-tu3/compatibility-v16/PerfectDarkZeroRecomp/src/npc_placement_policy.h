#pragma once
#include <array>
#include <cmath>
#include <span>
namespace pdz_population {
struct Point {float x{},y{},z{};};
inline bool Finite(Point p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool Separated(Point p,std::span<const Point> others,float radius=100.f) {
  if(!Finite(p))return false;
  for(auto q:others) {
    if(!Finite(q))return false;
    const float dx=p.x-q.x,dz=p.z-q.z;
    if(std::abs(p.y-q.y)<180.f && dx*dx+dz*dz<radius*radius)return false;
  }
  return true;
}
inline Point Candidate(Point origin,unsigned index) {
  constexpr std::array<Point,8> directions{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},
      {.707107f,0,.707107f},{-.707107f,0,-.707107f},{.707107f,0,-.707107f},{-.707107f,0,.707107f}}};
  const auto d=directions[index%8];const float radius=140.f+100.f*(index/8);
  return {origin.x+d.x*radius,origin.y,origin.z+d.z*radius};
}
}
