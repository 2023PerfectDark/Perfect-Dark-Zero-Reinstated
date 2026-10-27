#include "generated/default/perfectdarkzerorecomp_init.h"
#include "blood_decals.h"
#include "campaign_gore.h"
#include "mousehook.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <array>
#include <bit>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <chrono>

REXCVAR_DEFINE_BOOL(pdz_blood_decals, false, "Gameplay",
    "Experimental native blood stains and attached hit marks");
REXCVAR_DEFINE_INT32(pdz_gore_decal_minutes, 10, "Gameplay", "Blood decal lifetime in minutes (1-20)");
namespace {
constexpr uint32_t kBloodType=0x424C4F44;
constexpr uint32_t kScatteredType=0x424C4F45,kDripsType=0x424C4F46;
#include "blood_decal_pixels.inl"
constexpr uint32_t kWoundType=0xBD000000;
constexpr uint32_t kMaxWoundsPerBody=16;
bool BloodType(uint32_t type){return type==kBloodType||type==kScatteredType||type==kDripsType||(type&0xFF000000u)==kWoundType;}
std::array<uint32_t,3> textures{};
thread_local uint32_t drawingTexture=0;
thread_local bool drawing=false,creating=false;
thread_local uint32_t created=0;
thread_local uint32_t created_group=0;
using BloodClock=std::chrono::steady_clock;
struct TimedMark { uint32_t address=0,group=0; BloodClock::time_point born{}; };
std::array<TimedMark,160> timed_marks{};
unsigned marks=0;
std::array<std::array<float,3>,32> pools{};
unsigned pool_count=0;
bool Readable(uint32_t a,uint32_t n){
  auto* rt=rex::Runtime::instance();
  if(!rt||!a||!n||uint64_t(a)+n>0x100000000ull)return false;
  for(uint64_t p=a;p<uint64_t(a)+n;){
    auto* h=rt->memory()->LookupHeap(uint32_t(p));uint32_t protection=0;
    if(!h||!h->QueryProtect(uint32_t(p),&protection)||!(protection&rex::memory::kMemoryProtectRead))return false;
    p=(p/h->page_size()+1)*h->page_size();
  }return true;
}
void ForgetGroup(uint32_t group){
  for(auto& mark:timed_marks)if(mark.address&&mark.group==group){mark={};if(marks)--marks;}
}
void Expire(PPCContext ctx,uint8_t* base){
  const auto now=BloodClock::now();
  const auto life=std::chrono::minutes(std::clamp(REXCVAR_GET(pdz_gore_decal_minutes),1,20));
  for(auto& mark:timed_marks){
    if(!mark.address||now-mark.born<life)continue;
    const auto group=mark.group,address=mark.address;mark={};if(marks)--marks;
    if(!Readable(group,9760)||!BloodType(REX_LOAD_U32(group))||!REX_LOAD_U8(group+9756))continue;
    auto node=REX_LOAD_U32(group+9700);const auto end=REX_LOAD_U32(group+9712);
    bool found=false;
    for(unsigned i=0;i<100&&node!=end&&Readable(node,96);++i){
      if(node==address){found=true;break;}node=REX_LOAD_U32(node+84);
    }
    if(!found)continue;
    auto call=ctx;call.r1.u32-=512;if(!Readable(call.r1.u32,512))continue;
    const auto count=REX_LOAD_U32(group+9728);if(!count||count>100)continue;
    // Native linked-list erase: unlink this mark, return its node to the free
    // list and decrement the same count used by the native renderer.
    REX_STORE_U32(group+9728,count-1);
    call.r3.u64=call.r1.u32+128;call.r4.u64=group+16;call.r5.u64=address;
    sub_8222E328(call,base);
    if(count==1){call.r3.u64=group;sub_8222C530(call,base);}
    pool_count=0;
  }
}
float F(uint8_t* base,uint32_t a){return std::bit_cast<float>(REX_LOAD_U32(a));}
void F(uint8_t* base,uint32_t a,float f){REX_STORE_U32(a,std::bit_cast<uint32_t>(f));}
using Vec=std::array<float,3>;
Vec V(uint8_t* base,uint32_t a){return {F(base,a),F(base,a+4),F(base,a+8)};}
void V(uint8_t* base,uint32_t a,const Vec& v){for(unsigned i=0;i<3;++i)F(base,a+i*4,v[i]);}
bool Finite(const Vec& v){return std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1e7f;});}
Vec Sub(const Vec& a,const Vec& b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
float Dot(const Vec& a,const Vec& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec Cross(const Vec& a,const Vec& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
bool Unit(Vec& v){float n=std::sqrt(Dot(v,v));if(!Finite(v)||n<.0001f)return false;for(auto& x:v)x/=n;return true;}
uint32_t Texture(PPCContext ctx,uint8_t* base,unsigned kind=0){
  auto& texture=textures[kind];
  if(texture)return texture;
  static std::array<BloodClock::time_point,3> retryAfter{};
  if(BloodClock::now()<retryAfter[kind])return 0;
  retryAfter[kind]=BloodClock::now()+std::chrono::seconds(2);
  const unsigned extent=kind?256:128;
  ctx.r1.u32-=1024;if(!Readable(ctx.r1.u32,1024))return 0;
  ctx.r3.u64=extent;ctx.r4.u64=extent;ctx.r5.u64=1;ctx.r6.u64=1;
  ctx.r7.u64=0;ctx.r8.u64=0x18280086;ctx.r9.u64=1;ctx.r10.u64=3;
  sub_826DD008(ctx,base);const auto t=ctx.r3.u32;if(!t){REXLOG_ERROR("Blood texture allocation failed kind={}",kind);return 0;}
  const auto lock=ctx.r1.u32+128;REX_STORE_U32(lock,0);REX_STORE_U32(lock+4,0);
  ctx.r3.u64=t;ctx.r4.u64=0;ctx.r5.u64=lock;ctx.r6.u64=0;ctx.r7.u64=0;
  sub_826DCD48(ctx,base);
  const auto pitch=REX_LOAD_U32(lock),data=REX_LOAD_U32(lock+4);
  if(!data||pitch<extent*4||pitch>8192||!Readable(data,pitch*extent)){
    static unsigned failedLocks=0;
    if(failedLocks++<6)REXLOG_ERROR("Blood texture lock failed kind={} texture={:08X} pitch={} data={:08X}",kind,t,pitch,data);
    ctx.r3.u64=t;sub_826D2208(ctx,base);return 0;
  }
  // LockRect may return a physical-heap alias, not a base-relative address.
  auto* pixels=rex::Runtime::instance()->memory()->TranslateVirtual<uint8_t*>(data);
  const auto writePixel=[&](unsigned x,unsigned y,uint32_t argb){
    auto* p=pixels+y*pitch+x*4;
    p[0]=uint8_t(argb>>24);p[1]=uint8_t(argb>>16);
    p[2]=uint8_t(argb>>8);p[3]=uint8_t(argb);
  };
  // Procedural material, with a transparent irregular edge and darker centre.
  for(unsigned y=0;y<extent;++y)for(unsigned x=0;x<extent;++x){
    if(kind){writePixel(x,y,(kind==1?kScatteredPixels:kDripsPixels)[y*extent+x]);continue;}
    const float u=(x-63.5f)/60.f,v=(y-63.5f)/60.f;
    const float angle=std::atan2(v,u),radius=std::sqrt(u*u+v*v);
    const float edge=.87f+.065f*std::sin(angle*7)+.035f*std::cos(angle*13);
    const float coverage=std::clamp((edge-radius)*30.f,0.f,1.f);
    const uint32_t alpha=uint32_t(coverage*235),red=uint32_t(52+28*std::min(radius,1.f));
    writePixel(x,y,(alpha<<24)|(red<<16)|0x0203);
  }
  ctx.r3.u64=t;ctx.r4.u64=0;sub_826DCD60(ctx,base);
  REXLOG_INFO("Blood texture ready kind={} texture={:08X} pitch={} data={:08X}",kind,t,pitch,data);
  texture=t;return t;
}
// Query static world triangles around the hit, then intersect a downward
// vertical line with their interiors. No guessed floor height or flat plane.
bool Floor(PPCContext ctx,uint8_t* base,const Vec& p,Vec& out,Vec& normal){
  ctx.r1.u32-=32768;if(!Readable(ctx.r1.u32,32768))return false;
  const auto query=ctx.r1.u32+256,collector=query+32,array=query+128;
  V(base,query,p);REX_STORE_U32(collector,0x8209BE6C);
  REX_STORE_U32(collector+4,0);REX_STORE_U32(collector+8,128);REX_STORE_U32(collector+12,array);
  ctx.r3.u64=0;ctx.r4.u64=query;ctx.r6.s64=-1;ctx.r7.u64=collector;
  ctx.r8.u64=0;ctx.r9.u64=0;ctx.r10.u64=0;ctx.f1.f64=250;
  sub_8222E0B8(ctx,base);
  const auto count=std::min(REX_LOAD_U32(collector+4),128u);
  bool found=false;float best=-1e10f;
  for(unsigned i=0;i<count;++i){
    const auto at=array+i*104;const auto a=V(base,at),b=V(base,at+12),c=V(base,at+24);
    if(!Finite(a)||!Finite(b)||!Finite(c))continue;
    const float den=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);
    if(std::abs(den)<.001f)continue;
    const float u=((b[2]-c[2])*(p[0]-c[0])+(c[0]-b[0])*(p[2]-c[2]))/den;
    const float v=((c[2]-a[2])*(p[0]-c[0])+(a[0]-c[0])*(p[2]-c[2]))/den;
    if(u<0||v<0||u+v>1)continue;
    const float y=u*a[1]+v*b[1]+(1-u-v)*c[1];
    auto n=V(base,at+36);const float length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    if(!Finite(n)||length<.001f||std::abs(n[1])/length<.65f||y>p[1]+2||y<p[1]-240||y<=best)continue;
    for(auto& x:n)x/=length;if(n[1]<0)for(auto& x:n)x=-x;
    best=y;normal=n;out={p[0],y+.15f,p[2]};found=true;
  }return found;
}
// Verify the footprint against native collision, not just the centre hit.
// World marks must not be left attached to a moving prop or character.
bool WorldContact(PPCContext ctx,uint8_t* base,const Vec& p,const Vec& n){
  ctx.r1.u32-=2048;if(!Readable(ctx.r1.u32,2048))return false;
  const auto a=ctx.r1.u32+256,b=a+16,hit=b+16,point=hit+16,normal=point+16;
  Vec from=p,to=p;for(unsigned i=0;i<3;++i){from[i]+=n[i]*3.f;to[i]-=n[i]*3.f;}
  V(base,a,from);V(base,b,to);REX_STORE_U32(hit,0);V(base,normal,{});
  for(auto offset:{84,92,100})REX_STORE_U32(ctx.r1.u32+offset,0);
  ctx.r3.u64=0;ctx.r4.u64=a;ctx.r5.u64=b;ctx.r6.u64=4;
  ctx.r7.u64=hit;ctx.r8.u64=0;ctx.r9.u64=point;ctx.r10.u64=normal;
  sub_82239358(ctx,base);
  if(!ctx.r3.u8||REX_LOAD_U32(hit))return false;
  auto actual=V(base,normal);const auto delta=Sub(V(base,point),p);
  return Unit(actual)&&std::abs(Dot(actual,n))>.985f&&Dot(delta,delta)<1.f;
}
bool FitWorldMark(PPCContext ctx,uint8_t* base,const Vec& p,const Vec& n,float& size){
  if(!WorldContact(ctx,base,p,n))return false;
  Vec axis=std::abs(n[1])<.9f?Vec{0,1,0}:Vec{1,0,0};
  auto x=Cross(n,axis);if(!Unit(x))return false;const auto y=Cross(n,x);
  // Cover the circumscribed square so any randomized decal rotation fits.
  for(unsigned attempt=0;attempt<5&&size>=3.f;++attempt){
    bool supported=true;
    for(int row=-2;row<=2&&supported;++row)for(int col=-2;col<=2;++col){
      if(!row&&!col)continue;Vec sample=p;
      for(unsigned i=0;i<3;++i)sample[i]+=(x[i]*col+y[i]*row)*size*.707107f;
      if(!WorldContact(ctx,base,sample,n)){supported=false;break;}
    }
    if(supported)return true;
    size*=.65f;
  }
  return false;
}
void Mark(PPCContext ctx,uint8_t* base,uint32_t manager,uint32_t owner,const Vec& p,const Vec& n,float size,int bone=-1,uint32_t type=kBloodType){
  if(marks>=160||!Finite(p)||!Finite(n))return;
  if(!owner&&!FitWorldMark(ctx,base,p,n,size))return;
  ctx.r1.u32-=1024;if(!Readable(ctx.r1.u32,1024))return;
  const auto pos=ctx.r1.u32+256,normal=pos+16;V(base,pos,p);V(base,normal,n);
  ctx.r3.u64=manager;ctx.r4.u64=owner;ctx.r5.u64=0;ctx.r6.u64=pos;ctx.r7.u64=normal;
  ctx.r10.u64=type;ctx.f1.f64=.2;ctx.f2.f64=.5;
  creating=true;created=0;
  if(owner&&bone>=0){
    // A separate native group per owner, with the validated skeleton joint.
    // Register as world first to avoid the prop-only part-number lookup; native
    // owner teardown still discovers and removes this group through +9748.
    // Append to the actual owner's group, including after corpse transfer.
    // Do not rely on the truncated owner tag to identify an existing group.
    uint32_t group=0;
    const auto count=std::min(REX_LOAD_U32(manager+104),50u);
    for(unsigned i=0;i<count;++i){
      const auto candidate=manager+112+i*9760;
      if(Readable(candidate,9760)&&REX_LOAD_U8(candidate+9756)&&
         (REX_LOAD_U32(candidate)&0xFF000000u)==kWoundType&&
         REX_LOAD_U32(candidate+9748)==owner){group=candidate;break;}
    }
    if(group&&REX_LOAD_U32(group+9728)>=kMaxWoundsPerBody){creating=false;return;}
    if(!group){
      ctx.r4.u64=0;ctx.r5.u64=0;ctx.r6.u64=kWoundType|(owner&0x00FFFFFFu);
      sub_8222D270(ctx,base);
      const auto index=ctx.r3.s32;
      if(index>=0&&index<50)group=manager+112+uint32_t(index)*9760;
    }
    if(group){
      REX_STORE_U32(group+9748,owner);
      ctx.r3.u64=group;ctx.r4.u64=pos;ctx.r5.u64=normal;ctx.r8.s64=bone;
      ctx.f1.f64=.2;ctx.f2.f64=.5;sub_8222D990(ctx,base);
    }
  }else sub_8222CF70(ctx,base);
  creating=false;
  if(created&&Readable(created,80)){
    for(unsigned axis=0;axis<3;++axis){const auto at=created+axis*16;auto v=V(base,at);
      const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
      if(length>.00001f){for(auto& x:v)x*=size/length;V(base,at,v);}}
    if(type==kScatteredType||type==kDripsType){
      // Native quad vertices are (-1,-1,0)..(1,1,0), with UV (0,0)
      // at (-1,-1,0). Local Y therefore follows the downward texture axis.
      Vec up={0,1,0};const float projection=Dot(up,n);
      for(unsigned i=0;i<3;++i)up[i]-=n[i]*projection;
      if(!Unit(up)){up={0,0,-1};for(unsigned i=0;i<3;++i)up[i]-=n[i]*Dot(Vec{0,0,-1},n);if(!Unit(up))return;}
      // Native construction uses Y = normal cross X, so X cross Y must
      // point out of the surface. UV-down is -up: X must be normal cross up.
      // The opposite cross product reverses winding and gets backface-culled.
      auto right=Cross(n,up);if(!Unit(right))return;
      float angle=0;
      if(type==kScatteredType){
        static uint32_t random=0x7A93BD21;
        random^=random<<13;random^=random>>17;random^=random<<5;
        angle=float(random&0xFFFFFF)*(6.28318530718f/16777216.f);
      }
      const float c=std::cos(angle),sn=std::sin(angle);
      Vec x{},z{};for(unsigned i=0;i<3;++i){x[i]=(right[i]*c+up[i]*sn)*size;z[i]=(right[i]*sn-up[i]*c)*size;}
      V(base,created,x);V(base,created+16,z);V(base,created+32,n);
    }
    F(base,created+64,.9f);
    auto slot=std::find_if(timed_marks.begin(),timed_marks.end(),[](const auto& m){return !m.address;});
    if(slot!=timed_marks.end()){*slot={created,created_group,BloodClock::now()};++marks;}
    REXLOG_INFO("Blood mark created: owner={:08X} bone={} radius={}",owner,REX_LOAD_U32(created+68),size);
  }
}
// Trace the bullet continuation against nearby static collision triangles.
// A downward ray is a fallback when there is no wall behind the victim.
void SurfaceSplatter(PPCContext hit,uint8_t* base,uint32_t manager,const Vec& point,bool head){
  if(!Readable(hit.r6.u32,12))return;
  Vec direction=V(base,hit.r6.u32);if(!Unit(direction))return;
  auto call=hit;if(call.r1.u32<32768)return;call.r1.u32-=32768;
  if(!Readable(call.r1.u32,32768))return;
  const auto query=call.r1.u32+256,collector=query+32,array=query+128;
  V(base,query,point);REX_STORE_U32(collector,0x8209BE6C);
  REX_STORE_U32(collector+4,0);REX_STORE_U32(collector+8,128);REX_STORE_U32(collector+12,array);
  call.r3.u64=0;call.r4.u64=query;call.r6.s64=-1;call.r7.u64=collector;
  call.r8.u64=0;call.r9.u64=0;call.r10.u64=0;call.f1.f64=180;
  sub_8222E0B8(call,base);
  const auto count=std::min(REX_LOAD_U32(collector+4),128u);
  for(const Vec ray: {direction,Vec{0,-1,0}}){
    float best=180;Vec position{},normal{};bool found=false;
    for(unsigned i=0;i<count;++i){
      const auto at=array+i*104;const auto a=V(base,at),b=V(base,at+12),c=V(base,at+24);
      if(!Finite(a)||!Finite(b)||!Finite(c))continue;
      const auto e1=Sub(b,a),e2=Sub(c,a),h=Cross(ray,e2);const float det=Dot(e1,h);
      if(std::abs(det)<.00001f)continue;
      const auto delta=Sub(point,a);const float u=Dot(delta,h)/det;if(u<0||u>1)continue;
      const auto q=Cross(delta,e1);const float v=Dot(ray,q)/det;if(v<0||u+v>1)continue;
      const float distance=Dot(e2,q)/det;if(!std::isfinite(distance)||distance<1||distance>=best)continue;
      auto n=Cross(e1,e2);if(!Unit(n))continue;if(Dot(n,ray)>0)for(auto& x:n)x=-x;
      best=distance;normal=n;for(unsigned j=0;j<3;++j)position[j]=point[j]+ray[j]*distance+n[j]*.2f;
      found=true;
    }
    if(found){Mark(hit,base,manager,0,position,normal,head?25.f:30.f,-1,head?kDripsType:kScatteredType);return;}
  }
}
void Wounds(PPCContext hit,uint8_t* base,uint32_t manager,const Vec& world){
  if(!Readable(hit.r6.u32,12))return;
  const auto owner=hit.r3.u32,model=REX_LOAD_U32(owner+72);
  if(!Readable(model,80)||!REX_LOAD_U32(model+60))return; // No active skeleton.
  auto call=hit;call.r1.u32-=32768;if(!Readable(call.r1.u32,32768))return;
  const auto point=call.r1.u32+256,collector=point+32,adapter=point+64,array=point+128;
  V(base,point,world);REX_STORE_U32(collector,0x8209BE6C);
  REX_STORE_U32(collector+4,0);REX_STORE_U32(collector+8,128);REX_STORE_U32(collector+12,array);
  // Native owner-to-world triangle adapter, as constructed by 8222CCB0.
  REX_STORE_U32(adapter,0x82098EA0);REX_STORE_U32(adapter+4,0);
  REX_STORE_U8(adapter+8,0);REX_STORE_U32(adapter+12,collector);
  REX_STORE_U32(adapter+16,owner);REX_STORE_U8(adapter+20,1);
  call.r3.u64=model;call.r4.u64=point;call.r6.u64=adapter;call.f1.f64=100;
  sub_82515978(call,base);
  const auto count=std::min(REX_LOAD_U32(collector+4),128u);
  REXLOG_INFO("Blood character query: owner={:08X} triangles={}",owner,count);
  Vec direction=V(base,hit.r6.u32);const bool has_direction=Unit(direction);
  float entryDistance=1e10f,exitDistance=1e10f;
  Vec entry{},exit{},entryNormal{},exitNormal{};int entryBone=-1,exitBone=-1;
  // Intersect the shot line with actual collision triangles, in world space.
  for(unsigned i=0;has_direction&&i<count;++i){
    const auto at=array+i*104;const auto a=V(base,at),b=V(base,at+12),c=V(base,at+24);
    if(!Finite(a)||!Finite(b)||!Finite(c))continue;
    const auto e1=Sub(b,a),e2=Sub(c,a),h=Cross(direction,e2);const float det=Dot(e1,h);
    if(std::abs(det)<.00001f)continue;
    const auto s=Sub(world,a);const float u=Dot(s,h)/det;if(u<0||u>1)continue;
    const auto q=Cross(s,e1);const float v=Dot(direction,q)/det;if(v<0||u+v>1)continue;
    const float t=Dot(e2,q)/det;if(!std::isfinite(t))continue;
    auto n=Cross(e1,e2);if(!Unit(n))continue;
    const auto bone=REX_LOAD_U32(at+88);if(bone>255)continue;
    Vec p=world;for(unsigned j=0;j<3;++j)p[j]+=direction[j]*t;
    if(std::abs(t)<entryDistance&&std::abs(t)<6){entryDistance=std::abs(t);entry=p;entryNormal=n;if(Dot(entryNormal,direction)>0)for(auto& x:entryNormal)x=-x;entryBone=int(bone);}
    if(t>2&&t<exitDistance&&t<100){exitDistance=t;exit=p;exitNormal=n;if(Dot(exitNormal,direction)<0)for(auto& x:exitNormal)x=-x;exitBone=int(bone);}
  }
  auto place=[&](Vec p,Vec n,int bone,float size){
    if(bone<0)return;
    for(unsigned j=0;j<3;++j)p[j]+=n[j]*.2f;
    V(base,point,p);V(base,point+16,n);
    auto convert=hit;convert.r1.u32=call.r1.u32;convert.r3.u64=owner;convert.r4.u64=point;convert.r5.u64=point;
    sub_8250A810(convert,base);
    convert=hit;convert.r1.u32=call.r1.u32;convert.r3.u64=owner;convert.r4.u64=point+16;convert.r5.u64=point+16;
    sub_8250A908(convert,base);
    Mark(call,base,manager,owner,V(base,point),V(base,point+16),size,bone);
  };
  place(entry,entryNormal,entryBone,3.f);
  if(entryBone>=0)place(exit,exitNormal,exitBone,4.5f);
  if(entryBone<0){
    // Some characters expose collision volumes rather than mesh triangles.
    // Their native hit point remains valid; bind it to the closest animated
    // joint instead of silently producing no mark. No guessed exit is added.
    const auto definition=REX_LOAD_U32(model);
    if(!Readable(definition,16))return;
    const auto skeleton=REX_LOAD_U32(definition+12);
    if(!Readable(skeleton,56))return;
    const auto joints=REX_LOAD_U32(skeleton+48),bind=REX_LOAD_U32(skeleton+52);
    const auto frame=REX_LOAD_U32(0x82FBC670);
    if(joints==0||joints>256||frame>3||!Readable(bind,joints*36)||!Readable(model+60+frame*4,4))return;
    if(!Readable(REX_LOAD_U32(model+60+frame*4),joints*64))return;
    const auto local=V(base,hit.r4.u32);auto normal=V(base,hit.r5.u32);
    if(!Unit(normal))return;
    float best=6400;int joint=-1;
    for(unsigned i=0;i<joints;++i){
      auto pose=call;pose.r3.u64=model;pose.r4.u64=i;pose.r5.u64=point;pose.r6.u64=0;
      sub_82408AC8(pose,base);const auto centre=V(base,point+48);
      if(!Finite(centre))continue;const auto d=Sub(local,centre);const float distance=Dot(d,d);
      if(distance<best){best=distance;joint=int(i);}
    }
    if(joint>=0){auto p=local;for(unsigned i=0;i<3;++i)p[i]+=normal[i]*.8f;
      Mark(call,base,manager,owner,p,normal,4.f,joint);}
  }
}
}

namespace pdz_blood {
void Hit(PPCContext& hit,uint8_t* base,bool gore){
  if(!pdz_input::Supported()||(!gore&&!REXCVAR_GET(pdz_blood_decals)))return;
  Expire(hit,base);if(marks>=160)return;
  if(!Readable(hit.r3.u32,80)||!Readable(hit.r4.u32,12)||!Readable(hit.r5.u32,12))return;
  const auto resources=REX_LOAD_U32(0x82D6B2B0+48);
  if(!Readable(resources,52))return;const auto manager=REX_LOAD_U32(resources+48);
  if(!Readable(manager,112))return;
  const auto local=V(base,hit.r4.u32),normal=V(base,hit.r5.u32);
  if(!Finite(local)||!Finite(normal))return;
  auto call=hit;call.r1.u32-=1024;if(!Readable(call.r1.u32,1024))return;
  const auto world=call.r1.u32+256;call.r5.u64=world;sub_8250A7B8(call,base);
  Vec floor{},up{};const auto point=V(base,world);if(!Finite(point))return;
  SurfaceSplatter(hit,base,manager,point,gore||hit.r7.u8!=0);
  if(!pdz_gore::Campaign(base))return;
  Wounds(hit,base,manager,point);
  if(pool_count>=pools.size())return;
  for(unsigned i=0;i<pool_count;++i){float d=0;for(unsigned j=0;j<3;++j)d+=(point[j]-pools[i][j])*(point[j]-pools[i][j]);if(d<6400)return;}
  if(Floor(call,base,point,floor,up)){
    const auto before=marks;Mark(hit,base,manager,0,floor,up,24.f);
    if(marks>before)pools[pool_count++]=point;
  }
}
}

REX_EXTERN(__imp__sub_8222DE78);
REX_HOOK_RAW(sub_8222DE78){
  const auto out=ctx.r4.u32,group=ctx.r3.u32;__imp__sub_8222DE78(ctx,base);
  if(creating&&Readable(out,4)){
    created=REX_LOAD_U32(out);created_group=group;
    for(auto& mark:timed_marks)if(mark.address==created){mark={};if(marks)--marks;}
  }
}
REX_EXTERN(__imp__sub_8222C530);
REX_HOOK_RAW(sub_8222C530){
  ForgetGroup(ctx.r3.u32);__imp__sub_8222C530(ctx,base);
}
REX_EXTERN(__imp__sub_8222D0D8);
REX_HOOK_RAW(sub_8222D0D8){
  if(pdz_input::Supported())Expire(ctx,base);
  __imp__sub_8222D0D8(ctx,base);
}

// Corpse bullet impacts bypass the living-character blood callback. Their
// hit record has the same local position/normal and shot-direction data.
// Add only visual marks, before the stock handler moves the ragdoll; leave
// its impulse, effects and return value untouched.
REX_EXTERN(__imp__sub_8254A350);
REX_HOOK_RAW(sub_8254A350){
  const auto owner=ctx.r3.u32,record=ctx.r6.u32;
  if(pdz_input::Supported()&&REXCVAR_GET(pdz_blood_decals)&&
     Readable(owner,1020)&&REX_LOAD_U32(owner+12)==43&&
     REX_LOAD_U32(owner+1016)&&Readable(record,104)){
    auto hit=ctx;
    hit.r4.u64=record+68;
    hit.r5.u64=record+92;
    hit.r6.u64=record+40;
    hit.r7.u64=0; // This callback does not supply the living-hit head flag.
    pdz_blood::Hit(hit,base);
  }
  pdz_gore::CorpseHit(ctx,base);
}
REX_EXTERN(__imp__sub_825E1F28);
REX_HOOK_RAW(sub_825E1F28){
  if(drawing&&drawingTexture)ctx.r5.u64=drawingTexture;
  __imp__sub_825E1F28(ctx,base);
}
REX_EXTERN(__imp__sub_8222D678);
REX_HOOK_RAW(sub_8222D678){
  const auto group=ctx.r3.u32;
  const auto type=REX_LOAD_U32(group);
  if(pdz_input::Supported()&&BloodType(type)){
    static unsigned traces=0;
    if(traces++<8)REXLOG_INFO("Blood draw group={:08X} type={:08X} marks={} active={}",group,type,REX_LOAD_U32(group+9728),REX_LOAD_U8(group+9756));
    const auto tex=Texture(ctx,base,type==kScatteredType?1:type==kDripsType?2:0);if(!tex)return;
    const auto previousTexture=drawingTexture;drawingTexture=tex;
    REX_STORE_U32(group,2);const bool previous=drawing;drawing=true;
    __imp__sub_8222D678(ctx,base);
    drawing=previous;drawingTexture=previousTexture;REX_STORE_U32(group,type);return;
  }
  __imp__sub_8222D678(ctx,base);
}
REX_EXTERN(__imp__sub_8222D090);
REX_HOOK_RAW(sub_8222D090){
  __imp__sub_8222D090(ctx,base);
  if(pdz_input::Supported()){
    marks=pool_count=0;
    timed_marks={};pdz_gore::Reset();
    for(auto& texture:textures)if(texture){auto call=ctx;call.r3.u64=texture;texture=0;sub_826D2208(call,base);}
  }
}
REX_EXTERN(__imp__sub_8222D500);
REX_HOOK_RAW(sub_8222D500){
  std::array<uint32_t,50> hidden{};unsigned hidden_count=0;
  // The stock transfer assumes prop part tables. Character marks instead use
  // skeleton joints: transfer only when the replacement has the same model.
  if(pdz_input::Supported()&&Readable(ctx.r3.u32,112)){
    const auto manager=ctx.r3.u32,owner=ctx.r4.u32,replacement=ctx.r5.u32;
    const auto count=std::min(REX_LOAD_U32(manager+104),50u);
    bool compatible=false;
    if(Readable(owner,76)&&Readable(replacement,76)){
      const auto a=REX_LOAD_U32(owner+72),b=REX_LOAD_U32(replacement+72);
      if(Readable(a,64)&&Readable(b,64)&&REX_LOAD_U32(b+60)){
        const auto ad=REX_LOAD_U32(a),bd=REX_LOAD_U32(b);
        if(Readable(ad,16)&&Readable(bd,16)){
          const auto as=REX_LOAD_U32(ad+12),bs=REX_LOAD_U32(bd+12);
          compatible=ad==bd||(as!=0&&as==bs);
          // Corpse models may be separate resources with an identical rig.
          // Require an exact bind-joint table match, not just equal bone count.
          if(!compatible&&Readable(as,56)&&Readable(bs,56)){
            const auto n=REX_LOAD_U32(as+48),an=REX_LOAD_U32(as+52),bn=REX_LOAD_U32(bs+52);
            compatible=n>0&&n<=256&&n==REX_LOAD_U32(bs+48)&&Readable(an,n*36)&&Readable(bn,n*36)&&
                std::memcmp(base+an,base+bn,n*36)==0;
          }
        }
      }
    }
    for(unsigned i=0;i<count;++i){const auto group=manager+112+i*9760;
      if(!Readable(group,9760)||!BloodType(REX_LOAD_U32(group))||REX_LOAD_U32(group+9748)!=owner)continue;
      REXLOG_INFO("Blood mark model transfer: {:08X} -> {:08X}, compatible={}",owner,replacement,compatible);
      if(compatible){
        REX_STORE_U32(group+9748,replacement);
        if(replacement==owner&&REX_LOAD_U8(group+9756)){
          hidden[hidden_count++]=group;REX_STORE_U8(group+9756,0);
        }
      }
      else {auto call=ctx;call.r1.u32-=512;call.r3.u64=group;sub_8222C530(call,base);}
    }
  }
  __imp__sub_8222D500(ctx,base);
  for(unsigned i=0;i<hidden_count;++i)REX_STORE_U8(hidden[i]+9756,1);
}

REX_EXTERN(__imp__sub_8222D410);
REX_HOOK_RAW(sub_8222D410){
  if(pdz_input::Supported()&&Readable(ctx.r3.u32,112)){
    const auto manager=ctx.r3.u32,owner=ctx.r4.u32;
    const auto count=std::min(REX_LOAD_U32(manager+104),50u);
    for(unsigned i=0;i<count;++i){const auto group=manager+112+i*9760;
      if(Readable(group,9760)&&REX_LOAD_U8(group+9756)&&BloodType(REX_LOAD_U32(group))&&REX_LOAD_U32(group+9748)==owner)
        REXLOG_INFO("Blood mark owner cleanup: owner={:08X} caller={:08X}",owner,uint32_t(ctx.lr));
    }
  }
  __imp__sub_8222D410(ctx,base);
}

// DP_Corpse takes the dying entity in r4 and copies its model/pose into r3.
// Character death does not call the prop-fragment transfer (8222D500).
// Capture the model definition before construction can relinquish it, then
// move only our bone-attached groups to the constructed corpse. Its ordinary
// entity destructor will still remove those groups when the corpse expires.
REX_EXTERN(__imp__sub_82549BC8);
REX_HOOK_RAW(sub_82549BC8){
  const auto corpse=ctx.r3.u32,owner=ctx.r4.u32;
  uint32_t definition=0;
  if(pdz_input::Supported()&&Readable(owner,76)){
    const auto model=REX_LOAD_U32(owner+72);
    if(Readable(model,4))definition=REX_LOAD_U32(model);
  }
  __imp__sub_82549BC8(ctx,base);
  if(!definition||!Readable(corpse,76)||REX_LOAD_U32(corpse+12)!=43)return;
  const auto model=REX_LOAD_U32(corpse+72);
  if(!Readable(model,64)||REX_LOAD_U32(model)!=definition||!REX_LOAD_U32(model+60))return;
  const auto resources=REX_LOAD_U32(0x82D6B2B0+48);
  if(!Readable(resources,52))return;
  const auto manager=REX_LOAD_U32(resources+48);
  if(!Readable(manager,112))return;
  const auto count=std::min(REX_LOAD_U32(manager+104),50u);
  for(unsigned i=0;i<count;++i){
    const auto group=manager+112+i*9760;
    if(!Readable(group,9760)||!REX_LOAD_U8(group+9756)||
       (REX_LOAD_U32(group)&0xFF000000u)!=kWoundType||REX_LOAD_U32(group+9748)!=owner)continue;
    REX_STORE_U32(group+9748,corpse);
    REX_STORE_U32(group,kWoundType|(corpse&0x00FFFFFFu));
    REXLOG_INFO("Blood marks transferred to corpse: {:08X} -> {:08X}",owner,corpse);
  }
}
