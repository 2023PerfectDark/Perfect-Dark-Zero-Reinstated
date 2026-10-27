#include "../src/weather_areas.h"
#include <cassert>
int main(){
 using namespace pdz_weather_areas;
 const auto& a=areas[0];
 auto start=At(7,a.start);assert(start.snow&&start.breath);
 Point middle{(a.start.x+a.end.x)/2,(a.start.y+a.end.y)/2,(a.start.z+a.end.z)/2};
 auto mid=At(7,middle);assert(mid.snow&&mid.breath);
 auto stop=At(7,a.end);assert(!stop.snow&&stop.breath);
 auto otherMission=At(6,middle);assert(!otherMission.snow&&!otherMission.breath);
 auto beside=At(7,{middle.x,middle.y,middle.z+800});assert(!beside.snow&&!beside.breath);
 auto otherFloor=At(7,{middle.x,middle.y+1000,middle.z});assert(!otherFloor.snow&&!otherFloor.breath);
 auto farInside=At(7,{a.end.x-1500,a.end.y,a.end.z});assert(!farInside.snow&&!farInside.breath);
 auto secondStart=At(7,{16975.832f,1060.642f,11374.107f});assert(secondStart.snow&&secondStart.breath);
 auto secondStop=At(7,{-7849.266f,1024.809f,6680.065f});assert(!secondStop.snow&&secondStop.breath);
 auto liftTop=At(7,{-8445.442f,1024.809f,7508.804f});assert(!liftTop.snow&&liftTop.breath);
 auto downstairs=At(7,{-8445.442f,-5949.232f,7508.804f});assert(!downstairs.snow&&!downstairs.breath);
}
