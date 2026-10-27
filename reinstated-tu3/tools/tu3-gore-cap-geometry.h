#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace gore_cap {
using Vec=std::array<float,3>;
struct Profile {unsigned reference;bool reverse;float inset,radius,referenceLength;};
// Use the outgoing limb axis, not pelvis-to-hip (which points sideways).
constexpr Profile For(unsigned root){
  switch(root){
    case 17:return {13,true,3.f,4.6f,7.2f};
    case 18:return {34,false,1.5f,5.5f,29.5f};
    case 21:return {39,false,1.5f,5.5f,29.5f};
    case 2:return {5,false,0.f,8.5f,49.f};
    case 3:return {6,false,0.f,8.5f,49.f};
    default:return {256,false,0,0,1};
  }
}
struct Section {Vec centre{},normal{};float radius=0;};
inline Section Fit(unsigned root,const Vec& bindRoot,const Vec& bindParent,
                   const Vec& bindReference,const std::array<float,16>& parentPose){
  const auto profile=For(root);Section out;
  if(profile.reference==256)return out;
  Vec axis{};float length2=0;
  for(unsigned i=0;i<3;++i){axis[i]=(bindReference[i]-bindRoot[i])*(profile.reverse?-1.f:1.f);length2+=axis[i]*axis[i];}
  if(!std::isfinite(length2)||length2<.01f)return out;
  const float length=std::sqrt(length2),scale=std::clamp(length/profile.referenceLength,.75f,1.4f);
  for(auto& x:axis)x/=length;
  Vec local{};
  for(unsigned i=0;i<3;++i)local[i]=bindRoot[i]-bindParent[i]-axis[i]*profile.inset*scale;
  float normalLength2=0;
  for(unsigned i=0;i<3;++i){
    out.centre[i]=parentPose[12+i];
    for(unsigned k=0;k<3;++k){out.centre[i]+=parentPose[k*4+i]*local[k];out.normal[i]+=parentPose[k*4+i]*axis[k];}
    if(!std::isfinite(out.centre[i])||!std::isfinite(out.normal[i]))return {};
    normalLength2+=out.normal[i]*out.normal[i];
  }
  if(normalLength2<.001f)return {};
  for(auto& x:out.normal)x/=std::sqrt(normalLength2);
  out.radius=profile.radius*scale;
  return out;
}
}
