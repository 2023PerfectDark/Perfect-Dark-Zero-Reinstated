#pragma once
#include <cmath>
#include <cstdint>
namespace gore_policy {
enum class Part : uint8_t { None, Head, LeftArm, RightArm, LeftLeg, RightLeg };
// Retail 8254FC18 maps impact regions to the humanoid skeleton.
constexpr Part Region(unsigned region) {
  if(region==1||region==2)return Part::Head;
  if(region>=6&&region<=9)return Part::LeftArm;
  if(region>=10&&region<=13)return Part::RightArm;
  if(region>=14&&region<=16)return Part::LeftLeg;
  if(region>=17&&region<=19)return Part::RightLeg;
  return Part::None;
}
constexpr Part ImpactPart(unsigned weapon,unsigned region,unsigned roll,unsigned side) {
 const auto part=Region(region);
 if(weapon!=27)return part;
 if(part==Part::Head)return roll<10?part:Part::None;
 if(region>=3&&region<=5)return roll<25?(side%2?Part::RightArm:Part::LeftArm):Part::None;
 return part;
}
inline bool Allowed(unsigned weapon,Part part,float distanceSquared) {
  if(part==Part::None||!std::isfinite(distanceSquared)||distanceSquared<0)return false;
  switch(weapon){
    case 12:return distanceSquared<=500.f*500.f;
    case 11:return part==Part::Head&&distanceSquared<=2000.f*2000.f;
    case 37:return part==Part::Head;
    case 10:case 27:return true;
    default:return false;
  }
}
constexpr unsigned Root(Part part) {
  switch(part){case Part::Head:return 17;case Part::LeftArm:return 18;
    case Part::RightArm:return 21;case Part::LeftLeg:return 2;
    case Part::RightLeg:return 3;default:return 256;}
}
constexpr unsigned Bit(Part part){return part==Part::None?0:1u<<(unsigned(part)-1);}
constexpr float Radius(Part part){return part==Part::Head?7.f:
  (part==Part::LeftLeg||part==Part::RightLeg)?11.f:7.f;}
}
