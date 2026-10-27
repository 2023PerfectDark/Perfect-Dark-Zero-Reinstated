#pragma once
#include <cmath>

namespace pdz_weather_areas {
struct Point { float x,y,z; };
struct Area {
 unsigned mission;
 Point start,end;
 float half_width,height_tolerance,snow_end_inset,breath_extension;
};
// Captured Mission 7 passage, from the covered exterior to the indoor snow
// boundary. Add independently bounded areas here; never bypass roofs globally.
inline constexpr Area areas[]{
 {7,{13797.803f,1060.734f,8158.366f},{10290.884f,1740.824f,8241.427f},
  350.f,350.f,150.f,500.f},
 // Second upper-floor route; breath extends through the elevator approach.
 {7,{16975.832f,1060.642f,11374.107f},{-7849.266f,1024.809f,6680.065f},
  1000.f,350.f,150.f,1500.f}
};
struct Exposure { bool snow=false,breath=false; };
inline Exposure At(unsigned mission,Point p) {
 Exposure result;
 for(const auto& area:areas){
  if(area.mission!=mission)continue;
  const float dx=area.end.x-area.start.x,dz=area.end.z-area.start.z;
  const float length=std::sqrt(dx*dx+dz*dz);
  const float along=((p.x-area.start.x)*dx+(p.z-area.start.z)*dz)/length;
  const float across=((p.x-area.start.x)*dz-(p.z-area.start.z)*dx)/length;
  const float t=along<0?0:along>length?1:along/length;
  const float floor=area.start.y+(area.end.y-area.start.y)*t;
  if(std::abs(across)>area.half_width||std::abs(p.y-floor)>area.height_tolerance)continue;
  result.snow|=along>=-150.f&&along<length-area.snow_end_inset;
  result.breath|=along>=-150.f&&along<=length+area.breath_extension;
 }
 return result;
}
}
