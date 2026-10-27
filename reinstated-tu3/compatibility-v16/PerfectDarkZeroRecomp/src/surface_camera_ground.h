#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace pdz_surface {
struct CameraGroundTriangle {
  std::array<float,3> a,b,c;
  float min_x,max_x,min_z,max_z;
};
inline float CameraGroundHeight(const std::vector<CameraGroundTriangle>& mesh,
                               float x,float z,float fallback) {
  float highest=fallback;
  for(const auto& t:mesh) {
    if(x<t.min_x || x>t.max_x || z<t.min_z || z>t.max_z)continue;
    const double bx=t.b[0]-t.a[0],bz=t.b[2]-t.a[2];
    const double cx=t.c[0]-t.a[0],cz=t.c[2]-t.a[2];
    const double determinant=bx*cz-bz*cx;
    if(std::abs(determinant)<1e-6)continue;
    const double px=x-t.a[0],pz=z-t.a[2];
    const double u=(px*cz-pz*cx)/determinant;
    const double v=(bx*pz-bz*px)/determinant;
    if(u< -1e-5 || v< -1e-5 || u+v>1.00001)continue;
    const float y=float(t.a[1]+u*(t.b[1]-t.a[1])+v*(t.c[1]-t.a[1]));
    if(std::isfinite(y))highest=std::max(highest,y);
  }
  return highest;
}
inline float CameraMinimumHeight(const std::vector<CameraGroundTriangle>& mesh,
                                 float x,float z,float fallback) {
  // Sample a footprint rather than only the eye point, protecting the near
  // plane on sloping snow. Keep the camera at least 50 game units above it.
  float ground=fallback;
  for(float dx:{-24.f,0.f,24.f})for(float dz:{-24.f,0.f,24.f})
    ground=std::max(ground,CameraGroundHeight(mesh,x+dx,z+dz,fallback));
  return ground+50.f;
}
}
