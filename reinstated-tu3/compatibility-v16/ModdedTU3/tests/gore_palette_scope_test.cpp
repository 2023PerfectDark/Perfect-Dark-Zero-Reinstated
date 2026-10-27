#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>
#include "../src/gore_policy.h"

std::array<std::atomic<uint32_t>,2500> headless_bodies{};
std::array<unsigned,2500> removed_parts{};
std::vector<uint8_t> memory(0x80000);
uint32_t load32(uint32_t a){assert(a+4<=memory.size());return
 uint32_t(memory[a])<<24|uint32_t(memory[a+1])<<16|uint32_t(memory[a+2])<<8|memory[a+3];}
void store32(uint32_t a,uint32_t v){assert(a+4<=memory.size());for(unsigned i=0;i<4;++i)memory[a+i]=uint8_t(v>>(24-i*8));}
#define REX_LOAD_U32(a) load32(a)
#define REX_STORE_U32(a,v) store32(a,v)
#define REX_LOAD_U16(a) (uint16_t(memory[a])<<8|memory[(a)+1])
bool Readable(uint32_t a,uint32_t n){return a && uint64_t(a)+n<=memory.size();}
int ModelSlot(uint32_t m){return m>=0x1000 && (m-0x1000)%512==0 && (m-0x1000)/512<2500 ? (m-0x1000)/512 : -1;}
namespace pdz_gore {bool Campaign(uint8_t*){return true;}}
#include "old_palette.inc"
#include "new_palette.inc"

void setup(){
 std::fill(memory.begin(),memory.end(),0);for(auto& v:headless_bodies)v=0;removed_parts={};
 for(unsigned i=0;i<2;++i){
  uint32_t m=0x1000+i*512,d=0x5000+i*512,s=0x6000+i*128,b=0x7000+i*1024;
  store32(m,d);store32(d+4,0x9000+i*256);store32(d+8,20);store32(d+12,s);
  store32(s+48,20);store32(s+52,b);headless_bodies[i]=d;removed_parts[i]=1;
  for(unsigned j=0;j<20;++j){memory[b+j*36+24]=0;memory[b+j*36+25]=uint8_t(j);}
  for(unsigned f=0;f<2;++f){
   auto p=0x10000+i*0x10000+f*0x2000;store32(m+60+f*4,p);store32(m+32+f*4,0x40000+i*0x1000+f*0x400);
   for(unsigned j=0;j<20;++j)for(unsigned k:{0u,20u,40u,60u})store32(p+j*64+k,0x3f800000);
  }
 }
}
int main(){
 constexpr auto head=17*64;
 setup();auto before=memory;
 {
  new_HeadPalette draw(memory.data(),0x1000,0x9000,0x40400);
  assert(load32(0x12000+head)==0); // Selected render frame still hides the severed head.
  assert(load32(0x10000+head)==0x3f800000); // Simulation frame untouched.
  assert(load32(0x20000+head)==0x3f800000); // Unrelated model untouched.
 }
 assert(memory==before); // Exact restoration, no persistent deformation.
 {
  new_HeadPalette wrong(memory.data(),0x1000,0x9100,0x40400);assert(memory==before);
  new_HeadPalette stale(memory.data(),0x1000,0x9000,0x41400);assert(memory==before);
 }
 // Reproduce the old lifetime hazard: an unrelated, unpinned body is retired
 // during drawing and its palette storage becomes another weapon's material.
 setup();{
  old_HeadPalette draw(memory.data());store32(0x20000+head,0x01234560);
 }assert(load32(0x20000+head)==0x3f800000); // Old destructor poisons the new pointer.
 setup();{
  new_HeadPalette draw(memory.data(),0x1000,0x9000,0x40400);store32(0x20000+head,0x01234560);
 }assert(load32(0x20000+head)==0x01234560); // New code never saved/re-writes it.
 std::cout<<"PASS: old stale-palette corruption reproduced; selected-frame isolation, restoration and unrelated allocation reuse\n";
}
