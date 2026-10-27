// Executes the actual old/new generated sampler with checked guest memory and
// stubbed neighbouring functions. This tests control flow, not visual poses.
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
union PPCRegister { uint64_t u64{}; int64_t s64; uint32_t u32; int32_t s32; float f32; double f64; };
struct Compare {
  bool lt{},eq{},gt{};
  template<class T> void compare(T a,T b,int) { lt=a<b;eq=a==b;gt=a>b; }
};
struct PPCContext {
  PPCRegister r1,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r27,r28,r29,r30,r31,f1,f31,ctr;
  uint64_t lr{}; int xer{}; Compare cr6;
  struct {void disableFlushMode(){}} fpscr;
};
std::map<uint32_t,uint8_t> memory;
std::vector<std::string> calls;
uint32_t fault{};
bool valid(uint32_t p,unsigned n) {
  return (p>=0x1000 && uint64_t(p)+n<=0x21000) ||
      (p>=0x82E4234C && uint64_t(p)+n<=0x82E42350) ||
      (p>=0x82D1E910 && uint64_t(p)+n<=0x82D1E950);
}
uint64_t load(uint32_t p,unsigned n) {
  if(!valid(p,n)){fault=p;throw std::runtime_error("guest read");}
  uint64_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|memory[p+i];return v;
}
void store(uint32_t p,uint64_t v,unsigned n) {
  if(!valid(p,n))throw std::runtime_error("guest write");
  for(unsigned i=0;i<n;++i)memory[p+n-i-1]=uint8_t(v>>(8*i));
}
#define REX_FUNC_PROLOGUE()
#define REX_LOAD_U8(a) load(a,1)
#define REX_LOAD_U16(a) load(a,2)
#define REX_LOAD_U32(a) load(a,4)
#define REX_STORE_U32(a,v) store(a,v,4)
#define REX_STORE_U64(a,v) store(a,v,8)
#define REX_LOAD_U64(a) load(a,8)
#define REXLOG_WARN(...) ((void)0)
#define REX_CALL_INDIRECT_FUNC(a) calls.push_back("primary")
#define DEFINE_REX_FUNC(name) void name(PPCContext& ctx,uint8_t* base)
#define REX_HOOK_RAW(name) DEFINE_REX_FUNC(name)
void __savegprlr_27(PPCContext&,uint8_t*){}
void __restgprlr_27(PPCContext&,uint8_t*){calls.push_back("restore");}
void sub_82746A48(PPCContext& c,uint8_t*) {
  calls.push_back("prepare");store(c.r4.u32,0,2);store(c.r5.u32,0,4);
}
void sub_82720D90(PPCContext& c,uint8_t*) {
  calls.push_back("timing");store(c.r4.u32,0,4);
}
void sub_8271FAE0(PPCContext&,uint8_t*){calls.push_back("secondary");}
void sub_82746AC8(PPCContext&,uint8_t*){calls.push_back("unlock");}
#include "animation_channel_old.inc"
#include "animation_channel_new.inc"
using Sampler=void(*)(PPCContext&,uint8_t*);
std::vector<std::string> run(Sampler sample,bool channel,bool data,bool secondary,bool unlock) {
  memory.clear();calls.clear();fault=0;
  PPCContext c;c.r1.u32=0x20000;c.r3.u32=0x1000;
  store(0x1000,0x3000,4); // Animation state -> descriptor.
  store(0x3000+36,channel?0x4000:0,4);
  store(0x4000+24,data?0x6000:0,4);
  store(0x3000+68,secondary?0x7000:0,4);
  store(0x1000+172,unlock?1:0,4);
  // Native format-0 dispatch table entry (+24).
  store(0x82D1E910+24,0x8000,4);
  sample(c,nullptr);
  assert(c.r1.u32==0x20000);
  return calls;
}
int main() {
  for(bool secondary:{false,true})for(bool unlock:{false,true}) {
    for(bool data:{false,true})
      assert(run(old_sampler,true,data,secondary,unlock)==
             run(new_sampler,true,data,secondary,unlock));
    const auto expected=run(old_sampler,true,false,secondary,unlock);
    assert(run(new_sampler,false,false,secondary,unlock)==expected);
    bool crashed=false;
    try{run(old_sampler,false,false,secondary,unlock);}
    catch(const std::runtime_error&){crashed=true;}
    assert(crashed && fault==0x18);
  }
  std::cout<<"Native sampler regression passed: reproduces old 0x18 fault; "
              "new path preserves secondary sampling, cleanup and normal behavior\n";
}
