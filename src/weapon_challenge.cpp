#include "generated/default/perfectdarkzerorecomp_init.h"
#include "weapon_challenge.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <atomic>
#include <bit>
namespace {
std::atomic<unsigned> selected{0};
bool managing=false;
uint32_t initialized=0;
unsigned savedAmmo=0;
uint32_t savedProfile=0;
std::array<uint32_t,4> savedLoadout{};
bool Valid(uint32_t p,unsigned n) {
 auto* rt=rex::Runtime::instance();if(!rt||!p||uint64_t(p)+n>0x100000000ull)return false;
 for(uint64_t a=p;a<uint64_t(p)+n;) {auto* h=rt->memory()->LookupHeap(uint32_t(a));uint32_t prot=0;
 if(!h||!h->QueryProtect(uint32_t(a),&prot)||!(prot&rex::memory::kMemoryProtectRead))return false;
 a=(a/h->page_size()+1)*h->page_size();}return true;
}
bool Campaign(uint8_t* base) {if(!pdz_input::Supported()||!selected)return false;
 auto s=REX_LOAD_U32(0x82D6B2B0);return Valid(s,40)&&REX_LOAD_U32(s+36)==0;}
bool Player(uint8_t* base,uint32_t p) {auto l=REX_LOAD_U32(0x82E35468);
 return Campaign(base)&&p&&Valid(l,956)&&REX_LOAD_U32(l+952)==p;}
unsigned Id(uint8_t* base,uint32_t w) {if(!Valid(w,864)||REX_LOAD_U32(w+12)!=4)return 0;
 auto d=REX_LOAD_U32(w+860);return Valid(d,1148)?REX_LOAD_U32(d+1144):0;}
bool Restricted(unsigned id) {
 // Only challenge weapons are restricted. Zero is unarmed/non-weapon;
 // mission equipment outside this catalogue must retain its native behavior.
 return pdz_challenge::Find(id)&&id!=selected;
}
bool SharedAmmo(uint8_t* base,uint32_t weapon) {
 if(!Valid(weapon,864))return false;
 const auto source=REX_LOAD_U32(weapon+860);
 if(!Valid(source,200))return false;
 const auto ammo=REX_LOAD_U8(source+199);
 // Type 13 is the native no-ammunition placeholder (melee/energy items).
 if(ammo>=16||ammo==13)return false;
 const auto manager=REX_LOAD_U32(0x82D6B2B0+48);
 if(!Valid(manager,25))return false;
 const auto records=REX_LOAD_U32(manager);
 const unsigned count=REX_LOAD_U8(manager+24);
 if(!Valid(records,count*2196u))return false;
 for(unsigned i=0;i<count;++i){
  const auto def=REX_LOAD_U32(records+i*2196u);
  if(Valid(def,1148)&&REX_LOAD_U32(def+1144)==selected)
   return REX_LOAD_U8(def+199)==ammo;
 }
 return false;
}
}
namespace pdz_challenge {
void Select(unsigned id){selected=Find(id)?id:0;Reset();
 REXLOG_INFO("Weapon Only: selected weapon {}",selected.load());}
unsigned Selected(){return selected;}
void Reset(){initialized=0;savedAmmo=0;}
unsigned Limit(uint8_t* base){const auto* w=Find(selected);return w&&Campaign(base)?w->reserve:0;}
void ForceLoadout(uint8_t* base,uint32_t profile){
 if(!pdz_input::Supported()||!Valid(profile,9964))return;
 if(REX_LOAD_U32(profile+9960)!=REX_LOAD_U8(0x82D2CA58+20))return;
 const bool active=Campaign(base);
 if(savedProfile==profile&&!active){
  for(unsigned i=0;i<4;++i)REX_STORE_U32(profile+9444+i*4,savedLoadout[i]);
  savedProfile=0;
 }
 if(!active)return;
 if(savedProfile!=profile){
  savedProfile=profile;
  for(unsigned i=0;i<4;++i)savedLoadout[i]=REX_LOAD_U32(profile+9444+i*4);
 }
 // Populate the actual mission loadout, without requiring a campaign unlock.
 // Native mission startup loads assets from these slots before spawning weapons.
 for(unsigned i=0;i<4;++i)REX_STORE_U32(profile+9444+i*4,i==0?selected.load():0);
}
void Tick(PPCContext& ctx,uint8_t* base,uint32_t player){
 if(managing||!Player(base,player)||!Valid(player,6584)||pdz_surface::IntroCameraActive()||pdz_surface::EndingCameraActive())return;
 auto hp=REX_LOAD_U32(player+6560);
 if(!Valid(hp,980)||!(std::bit_cast<float>(REX_LOAD_U32(hp+968))>0))return;
 auto list=REX_LOAD_U32(player+5164),storage=REX_LOAD_U32(player+5212);
 if(!Valid(list,12)||!Valid(storage,104))return;
 auto nodes=REX_LOAD_U32(list),head=REX_LOAD_U32(list+8);
 if(!Valid(nodes,256)||(head&&(head<nodes||head>=nodes+256||(head-nodes)%8)))return;
 std::array<uint32_t,13> others{};unsigned count=0;uint32_t chosen=0,visited=0;
 for(int i=head?int((head-nodes)/8):-1;i!=-1;){if(i<0||i>=32||(visited&(1u<<i)))return;visited|=1u<<i;
 auto n=nodes+i*8,d=REX_LOAD_U32(n+4);if(d<storage||d>=storage+104||(d-storage)%8)return;
 auto w=REX_LOAD_U32(d),id=Id(base,w);
 if(id==selected)chosen=w;else if(Restricted(id)&&count<others.size())others[count++]=w;
 i=int16_t(REX_LOAD_U16(n+2));}
 managing=true;auto call=ctx;call.r1.u32-=2048;
 if(!Valid(call.r1.u32,2048)){managing=false;return;}
 if(!chosen){call.r3.u64=player;call.r4.u64=selected;call.r5.u64=0;call.r6.u64=0;call.r7.u64=0;call.r8.u64=0;
 sub_8253CCB8(call,base);chosen=call.r3.u32;
 if(Id(base,chosen)==selected){call.r3.u64=chosen;call.r4.u64=initialized==player?savedAmmo:Limit(base);call.r5.u64=1;sub_825746F0(call,base);}}
 if(Id(base,chosen)==selected){
 const bool firstEquip=initialized!=player;
 const bool replaceHeld=Restricted(Id(base,REX_LOAD_U32(player+4076)))||
                        Restricted(Id(base,REX_LOAD_U32(player+4080)));
 if(initialized!=player){call.r3.u64=chosen;call.r4.u64=Limit(base);call.r5.u64=1;sub_825746F0(call,base);
 call.r3.u64=chosen;call.r4.u64=65535;call.r5.u64=1;sub_825744A8(call,base);initialized=player;
 REXLOG_INFO("Weapon Only: player {:08X}, weapon {}, reserve {}",player,selected.load(),Limit(base));}
 for(unsigned i=0;i<count;++i){call.r3.u64=player;call.r4.u64=others[i];sub_8253CB78(call,base);}
 if((firstEquip||replaceHeld)&&REX_LOAD_U32(player+4076)!=chosen&&REX_LOAD_U32(player+4080)!=chosen){call.r3.u64=player;call.r4.u64=chosen;call.r5.u64=0;sub_8253E5A0(call,base);}
 const auto inv=REX_LOAD_U32(chosen+1100),def=REX_LOAD_U32(chosen+860);
 const auto ammo=REX_LOAD_U8(def+199);savedAmmo=Valid(inv,84)&&ammo<16?REX_LOAD_U16(inv+52+ammo*2):REX_LOAD_U16(chosen+1318);
 }
 managing=false;
}
}
// The challenge grants temporary access even when the profile has never
// collected this weapon. Do not set the permanent unlock/progression bits.
REX_EXTERN(__imp__sub_825001E8);
REX_HOOK_RAW(sub_825001E8){
 if(Campaign(base)&&ctx.r4.u32==selected&&Valid(ctx.r3.u32,9964)&&
    REX_LOAD_U32(ctx.r3.u32+9960)==REX_LOAD_U8(0x82D2CA58+20)){
  ctx.r3.u64=1;return;
 }
 __imp__sub_825001E8(ctx,base);
}
REX_EXTERN(__imp__sub_82574D68);
REX_HOOK_RAW(sub_82574D68){
 if(Player(base,ctx.r4.u32)&&Restricted(Id(base,ctx.r3.u32))&&!SharedAmmo(base,ctx.r3.u32)){ctx.r3.u64=0;return;}
 __imp__sub_82574D68(ctx,base);
}
REX_EXTERN(__imp__sub_825769B0);
REX_HOOK_RAW(sub_825769B0){
 const auto weapon=ctx.r3.u32,player=ctx.r4.u32;
 const bool ammoOnly=Player(base,ctx.r4.u32)&&Restricted(Id(base,ctx.r3.u32));
 const bool matching=ammoOnly&&SharedAmmo(base,ctx.r3.u32);
 const auto prompt=ctx.r5.u32,takeWeapon=ctx.r6.u32,takeAmmo=ctx.r7.u32;
 __imp__sub_825769B0(ctx,base);
 if(ammoOnly){
  const bool eligible=(Valid(prompt,1)&&REX_LOAD_U8(prompt))||
                      (Valid(takeWeapon,1)&&REX_LOAD_U8(takeWeapon))||
                      (Valid(takeAmmo,1)&&REX_LOAD_U8(takeAmmo));
  bool transfer=false;
  if(matching&&eligible&&Valid(player,5248)) {
   const auto def=REX_LOAD_U32(weapon+860);
   const auto ammo=REX_LOAD_U8(def+199);
   const auto reserve=REX_LOAD_U16(player+5164+52+ammo*2);
   transfer=reserve<pdz_challenge::Limit(base);
  }
  // Preserve native reserve-cap checks, pickup quantities, sound and depletion.
  // Suppress only weapon acquisition and its swap prompt.
  if(Valid(prompt,1))REX_STORE_U8(prompt,0);
  if(Valid(takeWeapon,1))REX_STORE_U8(takeWeapon,0);
  // With free inventory slots the retail check prefers acquiring an unowned
  // gun and clears takeAmmo. Convert that eligible pickup to ammo-only instead.
  if(Valid(takeAmmo,1))REX_STORE_U8(takeAmmo,transfer?1:0);
 }
}
REX_EXTERN(__imp__sub_8259F228);
REX_HOOK_RAW(sub_8259F228){
 if(Player(base,ctx.r3.u32)){
  // Scripted unarmed/gadget transitions are legitimate. Substitute only guns.
  if(Restricted(ctx.r4.u32))ctx.r4.u64=selected;
  if(Restricted(ctx.r5.u32))ctx.r5.u64=0;
 }
 __imp__sub_8259F228(ctx,base);
}

REX_EXTERN(__imp__sub_8253CCB8);
REX_HOOK_RAW(sub_8253CCB8){
 if(!managing&&Player(base,ctx.r3.u32)&&ctx.r4.u32&&pdz_challenge::Find(ctx.r4.u32)) {
  const auto player=ctx.r3.u32;
  for(auto offset:{4076u,4080u}) {
   const auto held=REX_LOAD_U32(player+offset);
   if(Id(base,held)==selected){ctx.r3.u64=held;return;}
  }
  ctx.r4.u64=selected;
 }
 __imp__sub_8253CCB8(ctx,base);
}
