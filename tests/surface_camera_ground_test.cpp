#include "../src/surface_camera_ground.h"
#include <cassert>
#include <iostream>
using namespace pdz_surface;
int main(){
  const CameraGroundTriangle slope{{0,0,0},{100,100,0},{0,0,100},0,100,0,100};
  std::vector<CameraGroundTriangle> mesh{slope};
  assert(std::abs(CameraGroundHeight(mesh,25,25,-100)-25)<.001f);
  assert(CameraGroundHeight(mesh,120,120,-100)==-100);
  assert(CameraMinimumHeight(mesh,25,25,0)>=99); // Forward footprint on slope.
  auto reverse=slope;std::swap(reverse.b,reverse.c);
  assert(std::abs(CameraGroundHeight({reverse},25,25,-100)-25)<.001f);
  auto upper=slope;upper.a[1]+=200;upper.b[1]+=200;upper.c[1]+=200;
  mesh.push_back(upper);
  assert(std::abs(CameraGroundHeight(mesh,25,25,-100)-225)<.001f);
  mesh.push_back({{0,0,0},{0,10,0},{0,20,0},0,0,0,0});
  assert(std::isfinite(CameraMinimumHeight(mesh,0,0,0)));
  assert(CameraMinimumHeight({},100,100,72)==122);
  std::cout<<"Surface camera slope, edge, winding, stacked ground and degenerate tests passed\n";
}
