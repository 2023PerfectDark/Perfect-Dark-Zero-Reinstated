#include "../src/inspection_motion.h"
#include <cassert>

int main() {
  for(unsigned kind: {1u,2u}) {
    for(float end:{0.f,1.f}) {
      auto k=pdz_weapons::InspectionMotion(kind,end);
      assert(k.pitch==0 && k.yaw==0 && k.roll==0);
      assert(k.x==0 && k.y==0 && k.z==0);
    }
    float previous=-1;
    for(int i=0;i<=10000;++i) {
      auto k=pdz_weapons::InspectionMotion(kind,float(i)/10000);
      assert(std::isfinite(k.roll) && std::isfinite(k.pose));
      assert(k.pose>=previous && k.pose>=0 && k.pose<=1);
      assert(std::abs(k.roll)<=45 && std::abs(k.yaw)<=30);
      previous=k.pose;
    }
  }
}
