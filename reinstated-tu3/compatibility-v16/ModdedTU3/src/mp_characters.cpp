#include "mp_characters.h"
#include "klobb_weapon.h"
#include "generated/default/pdzstocktu3_init.h"
#include "mousehook.h"
#include "utils.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <array>
#include <mutex>
#include <fstream>
#include <random>
#include <set>
#include <algorithm>
#include <cstring>

namespace {
bool Valid(uint32_t p,unsigned n) {
 auto* r=rex::Runtime::instance();if(!r||!p||uint64_t(p)+n>0x100000000ull)return false;
 for(uint64_t a=p;a<uint64_t(p)+n;){auto* h=r->memory()->LookupHeap(uint32_t(a));uint32_t prot=0;
 if(!h||!h->QueryProtect(uint32_t(a),&prot)||!(prot&rex::memory::kMemoryProtectRead))return false;
 a=(a/h->page_size()+1)*h->page_size();}return true;
}
bool Multiplayer(uint8_t* base){if(!pdz_input::Supported())return false;auto s=REX_LOAD_U32(0x82D6BFD8);
 return Valid(s,44)&&(REX_LOAD_U32(s+40)==2||REX_LOAD_U32(s+40)==3);}
bool NightclubCampaign(uint8_t* base){
 if(!pdz_input::Supported())return false;
 const auto session=REX_LOAD_U32(0x82D6BFD8);
 return Valid(session,44)&&REX_LOAD_U32(session+40)<=1&&REX_LOAD_U8(session+25)==1;
}
std::mutex settingsMutex;
struct Settings { bool armor=true;std::array<unsigned,2> teams{};std::array<unsigned,32> choices{},heads{},independent{};std::array<std::array<unsigned,32>,2> teamChoices{}; } settings;
std::once_flag loaded;
void Load(){std::call_once(loaded,[]{std::ifstream f("mp-characters.cfg");unsigned version=0;f>>version;
 if(version<1||version>4)return;Settings s;unsigned armor=1;f>>armor>>s.teams[0]>>s.teams[1];
 for(auto& c:s.choices)f>>c;
 if(version>=2)for(auto& team:s.teamChoices)for(auto& c:team)f>>c;
 if(version>=3)for(auto& c:s.heads)f>>c;
 if(version>=4)for(auto& c:s.independent)f>>c;
 if(!f||armor>1||s.teams[0]>2||s.teams[1]>2)return;
 for(auto c:s.choices)if(c>pdz_mp::Characters().size())return;
 for(auto c:s.heads)if(c>pdz_mp::Characters().size())return;
 for(auto c:s.independent)if(c>1)return;
 for(auto& team:s.teamChoices)for(auto c:team)if(c>pdz_mp::Characters().size())return;
 s.armor=armor!=0;settings=s;});}
void Save(){std::ofstream f("mp-characters.cfg",std::ios::trunc);f<<4<<' '<<settings.armor<<' '<<settings.teams[0]<<' '<<settings.teams[1];
 for(auto c:settings.choices)f<<' '<<c;for(auto& team:settings.teamChoices)for(auto c:team)f<<' '<<c;for(auto c:settings.heads)f<<' '<<c;for(auto c:settings.independent)f<<' '<<c;f<<'\n';}
uint32_t Definition(uint8_t* base,unsigned id,bool head=false){auto m=REX_LOAD_U32(0x82D6BFE0);
 if(!Valid(m,16))return 0;auto v=REX_LOAD_U32(m+(head?12:0));if(!Valid(v,12))return 0;
 auto count=REX_LOAD_U32(v+8),items=REX_LOAD_U32(v);if(count>256||!Valid(items,count*4))return 0;
 for(unsigned i=0;i<count;++i){auto d=REX_LOAD_U32(items+i*4);if(Valid(d,head?32:72)&&REX_LOAD_U32(d)==id)return d;}return 0;}
std::vector<uint32_t> Owners(uint8_t* base){std::vector<uint32_t> out;
 // The session roster starts with Player 1. The controller-list head at
 // 82E35468 instead points at the most recently created bot.
 std::set<uint32_t> seen;
 for(auto p=REX_LOAD_U32(0x82E34224);p&&seen.size()<32&&Valid(p,968)&&seen.insert(p).second;p=REX_LOAD_U32(p+4))
  out.push_back(p);
 return out;}
unsigned Team(PPCContext& original,uint8_t* base,uint32_t owner){if(!Valid(owner,204))return 0;
 auto state=REX_LOAD_U32(owner+80);if(!Valid(state,124))return 0;
 auto p=state+120,v=REX_LOAD_U32(p);if(!Valid(v,28))return 0;auto fn=REX_LOAD_U32(v+24);
 if(fn<0x82000000||fn>=0x82B00000)return 0;auto ctx=original;ctx.r1.u32-=512;
 if(!Valid(ctx.r1.u32,512))return 0;ctx.r3.u64=p;
 REX_CALL_INDIRECT_FUNC(fn);return ctx.r3.u32;}
bool NativeFreeChoice(PPCContext original,uint8_t* base){original.r1.u32-=512;
 if(!Valid(original.r1.u32,512))return false;sub_824430A0(original,base);return original.r3.u8==0;}
}
namespace pdz_mp {
bool FreeChoice(PPCContext& ctx,uint8_t* base){return NativeFreeChoice(ctx,base);}
const std::vector<Character>& Characters(){static const std::vector<Character> list{
 {u"Joanna - Bounty Hunter",110,157,110},{u"Joanna - Adventurer",85,120,85},
 {u"Joanna - Carrington",90,125,90},{u"Joanna - Black Leather",6,101,6},
 {u"Jack Dark",113,160,113},{u"Jack Dark - Campaign",7,102,7},{u"Chandra",115,162,115},
 {u"Zhang Li",11,104,103},{u"Zhang Li - Reborn",12,105,103},
 {u"Mai Hem - Holomatch",13,106,111},{u"Mai Hem",111,106,111},{u"Mai Hem - Burnt",94,107,94},
 {u"Killian",114,161,114},{u"Gygax",18,109,103},{u"Dr. Carroll",106,110,106},
 {u"Jonathan",112,159,112},{u"Daniel Carrington",28,112,103},
 {u"Brother Clay",24,113,103},{u"Brother Virgil",26,114,103},{u"Ringleader",99,115,99},
 {u"Gangster",104,2,104},{u"Zhang Li Henchman",50,8,103},{u"Zhang Li Henchman 2",103,9,103},
 {u"Private Force A",95,14,95},{u"Private Force B",96,17,96},{u"Private Force Female",54,20,115},
 {u"dataDyne Urban Trooper",102,23,102},{u"dataDyne Jungle Trooper",56,29,102},
 {u"dataDyne Desert Trooper",57,35,102},{u"Environmental Scientist",58,118,107},
 {u"Civilian Scientist",107,41,107},{u"Archaeologist",60,44,103},{u"Warehouse Worker",62,53,103},
 {u"Carrington SWAT",64,59,64},{u"Carrington Trooper",92,65,92},
 {u"Trinity Guard",67,71,102},{u"Trinity Worker",69,77,103},{u"Trinity Worker 2",70,78,103},
 {u"Party Guest - Male",71,83,103},{u"Party Guest - Female",73,86,115},
 {u"Biker",100,89,100},{u"Biker 2",76,90,100},{u"Subway Gang",77,95,103},
 {u"Nightclub Guest",79,116,103},{u"Nightclub Guest 2",81,116,103},
 // Match the retail nightclub actor setups; head 117 names an absent asset.
 {u"Nightclub Guest - Short Dress",80,22,110},{u"Nightclub Guest - Long Dress",82,20,115},
 {u"Nightclub Staff",83,116,103},
 // Head 119 references trooper_virtual_head, which is absent from retail
 // packages. The shipped Virtual Trooper package supplies soldier head 3.
 {u"Virtual Trooper",108,67,108},
 // Append new combinations so existing saved character choices keep their IDs.
 // A separate definition keeps Joanna's neckline apart from the guest outfit.
 {u"Joanna - Short Dress",109,119,110}
 };return list;}
unsigned Choice(unsigned slot,int team){Load();std::lock_guard l(settingsMutex);return slot<32?(team>=0&&team<2?settings.teamChoices[team][slot]:settings.choices[slot]):0;}
void Choose(unsigned slot,unsigned choice,int team){Load();std::lock_guard l(settingsMutex);if(slot>=32||choice>Characters().size())return;(team>=0&&team<2?settings.teamChoices[team][slot]:settings.choices[slot])=choice;
 if(team<0){settings.teamChoices[0][slot]=0;settings.teamChoices[1][slot]=0;}Save();}
unsigned TeamMode(unsigned team){Load();std::lock_guard l(settingsMutex);return team<2?settings.teams[team]:0;}
unsigned HeadChoice(unsigned slot){Load();std::lock_guard l(settingsMutex);return slot<32?settings.heads[slot]:0;}
bool IndependentParts(unsigned slot){Load();std::lock_guard l(settingsMutex);return slot<32&&settings.independent[slot];}
void ChoosePart(unsigned slot,unsigned choice,bool head){
 Load();std::lock_guard l(settingsMutex);if(slot>=32||choice>Characters().size())return;
 if(!settings.independent[slot]&&settings.choices[slot]&&!settings.heads[slot])settings.heads[slot]=settings.choices[slot];
 settings.independent[slot]=1;
 (head?settings.heads[slot]:settings.choices[slot])=choice;
 settings.teamChoices[0][slot]=settings.teamChoices[1][slot]=0;Save();
}
std::vector<unsigned> PartChoices(bool head,unsigned gender){
 std::vector<unsigned> result;std::set<unsigned> seen;
 const auto& list=Characters();
 for(unsigned i=0;i<list.size();++i){const auto& c=list[i];
  // Explicit roster classification; never infer gender from a display name.
  const bool female=c.body==110||c.body==85||c.body==90||c.body==6||c.body==115||
   c.body==13||c.body==111||c.body==94||c.body==54||c.body==73||c.body==80||c.body==82||c.body==109;
  if((gender==1&&female)||(gender==2&&!female))continue;
  if(seen.insert(head?c.head:c.body).second)result.push_back(i+1);
 }return result;
}
void RandomPart(unsigned slot,bool head,unsigned gender){
 auto choices=PartChoices(head,gender);if(choices.empty())return;
 const auto current=head?HeadChoice(slot):Choice(slot);
 if(current&&current<=Characters().size()&&choices.size()>1){
  const auto& previous=Characters()[current-1];
  std::erase_if(choices,[&](unsigned n){const auto& candidate=Characters()[n-1];
   return head?candidate.head==previous.head:candidate.body==previous.body;});
 }
 if(choices.empty())return;
 static std::mutex rngMutex;static std::mt19937 rng{std::random_device{}()};unsigned choice;
 {std::lock_guard l(rngMutex);choice=choices[std::uniform_int_distribution<size_t>(0,choices.size()-1)(rng)];}
 ChoosePart(slot,choice,head);
}
void SetTeamMode(unsigned team,unsigned mode){Load();std::lock_guard l(settingsMutex);if(team>=2||mode>2)return;settings.teams[team]=mode;Save();}
bool ArmorVisible(){Load();std::lock_guard l(settingsMutex);return settings.armor;}
void SetArmorVisible(bool visible){Load();std::lock_guard l(settingsMutex);settings.armor=visible;Save();}
std::vector<Slot> Slots(PPCContext& ctx,uint8_t* base){std::vector<Slot> out;auto owners=Owners(base);
 // Bots are created after leaving the lobby. Expose their future slots too.
 for(unsigned i=0;i<32;++i){auto n=std::to_string(i);std::u16string name=i?u"Bot / additional player ":u"Player 1";if(i)name.append(n.begin(),n.end());
 out.push_back({i,i<owners.size()?Team(ctx,base,owners[i]):0,name});}return out;}
void HideArmor(uint8_t* base,uint32_t object,uint32_t model){
 if(ArmorVisible()||!Multiplayer(base)||!Valid(object,972)||REX_LOAD_U32(object+12)!=25)return;
 auto kind=REX_LOAD_U32(object+968);if(kind!=1&&kind!=2)return;
 if(model<0x82E75160u||(model-0x82E75160u)%512u||(model-0x82E75160u)/512u>=2500u)return;
 auto e=0x82D73900u+((model-0x82E75160u)/512u)*8u;if(Valid(e,8))REX_STORE_U32(e,REX_LOAD_U32(e)&~0xffffu);
}
void RecoverRingleaderHeadShader(PPCContext& ctx,uint8_t* base,uint32_t graph){
 // The extracted head can reach rendering with uncreated shader slots.
 // Recover the exact serialized shader through the native constructor; the
 // model retains ownership and its normal teardown releases the new handle.
 if(!graph||!Multiplayer(base))return;
 const auto slot=ctx.r11.u32;
 if(slot<graph+32||uint64_t(slot)+4>uint64_t(graph)+84104||!Valid(slot-32,36))return;
 bool ringleader=false;
 for(auto owner:Owners(base)){
  const auto actor=REX_LOAD_U32(owner+964);
  if(!Valid(actor,5288)||REX_LOAD_U32(actor+5284)!=115)continue;
  const auto model=REX_LOAD_U32(actor+4104);
  if(!Valid(model,4))continue;
  const auto resource=REX_LOAD_U32(model);
  if(Valid(resource,8)&&REX_LOAD_U32(resource+4)==graph){ringleader=true;break;}
 }
 if(!ringleader||REX_LOAD_U32(slot))return;
 const auto shader=REX_LOAD_U32(slot-32);
 if(!Valid(shader,12)||REX_LOAD_U32(shader)!=0x102a0e01)return;
 const auto offset=REX_LOAD_U32(shader+4),size=REX_LOAD_U32(shader+8);
 if(offset<12||offset>65536||!size||size>65536||!Valid(shader,offset+size))return;
 static thread_local uint32_t attemptedGraph=0;
 static thread_local std::set<uint32_t> attempted;
 if(attemptedGraph!=graph){attemptedGraph=graph;attempted.clear();}
 if(attempted.size()>=128||!attempted.insert(slot).second)return;
 auto call=ctx;
 if(call.r1.u32<1024||!Valid(call.r1.u32-1024,1024))return;
 call.r1.u32-=512;call.r3.u64=shader;
 sub_826E4108(call,base);
 if(call.r3.u32){
  REX_STORE_U32(slot,call.r3.u32);ctx.r4.u64=call.r3.u32;
  REXLOG_INFO("Ringleader head shader recovered: graph={:08X} slot={:08X} shader={:08X}",graph,slot,call.r3.u32);
 }else REXLOG_WARN("Ringleader head shader creation failed: graph={:08X} slot={:08X} bytes={}",graph,slot,size);
}
}

namespace {
struct Applied {uint32_t owner=0,actor=0;unsigned choice=0;};
std::array<Applied,32> applied{};
thread_local const pdz_mp::Character* refreshCharacter=nullptr;
unsigned Selected(PPCContext& ctx,uint8_t* base,unsigned slot,uint32_t owner,uint32_t actor){
 const auto team=Team(ctx,base,owner),mode=team<2?pdz_mp::TeamMode(team):0;
 unsigned choice=pdz_mp::Choice(slot);
 // Individual customization takes precedence over team appearance policies.
 if(choice||pdz_mp::HeadChoice(slot))return choice;
 if(mode==0&&!NativeFreeChoice(ctx,base))return 0;
 if(mode==2){auto teamChoice=pdz_mp::Choice(slot,int(team));if(teamChoice)choice=teamChoice;}
 if(mode==1){
  auto& a=applied[slot];
  if(a.owner==owner&&a.actor==actor&&a.choice)return a.choice;
  static std::mt19937 random{std::random_device{}()};choice=1+random()%pdz_mp::Characters().size();
 }
 return choice;
}
uint32_t DressHandsName(uint8_t* base,bool longDress){
 // A separate loose asset preserves the normal Bounty Hunter viewmodel.
 // The native definition retains this string for the runtime's lifetime.
 const char* name=longDress?"aid_darkcharacter_xenonhands_longdress_hands":"aid_darkcharacter_xenonhands_shortdress_hands";
 const auto length=std::strlen(name)+1;
 std::error_code error;
 const auto file=utils::RepoRoot()/"assets"/"pdz"/name/"default.dbm";
 if(!std::filesystem::is_regular_file(file,error)||error)return 0;
 static std::mutex mutex;
 static uint32_t addresses[2]={};
 std::lock_guard lock(mutex);
 auto& address=addresses[longDress?1:0];
 if(!Valid(address,length)){
  address=rex::Runtime::instance()->memory()->SystemHeapAlloc(length,16);
  if(!address)return 0;
  std::memcpy(base+address,name,length);
 }
 return address;
}
bool PrepareJoannaDress(uint8_t* base){
 static std::mutex mutex;
 std::lock_guard lock(mutex);
 constexpr char name[]="aid_darkcharacter_xenonbodies_jo_shortdress_body";
 std::error_code error;
 if(!std::filesystem::is_regular_file(utils::RepoRoot()/"assets"/"pdz"/name/"default.dbm",error)||error)return false;
 const auto source=Definition(base,80),target=Definition(base,109);
 if(!Valid(source,972)||!Valid(target,72))return false;
 const auto currentName=REX_LOAD_U32(target+4);
 if(Valid(currentName,sizeof(name))&&!std::memcmp(base+currentName,name,sizeof(name)))return true;
 // Retail body 109 is an unused placeholder. Replace its vector entry with a
 // separate copy rather than modifying body 80, which the original guest uses.
 const auto defs=REX_LOAD_U32(0x82D6BFE0),vec=REX_LOAD_U32(defs),items=REX_LOAD_U32(vec),count=REX_LOAD_U32(vec+8);
 if(count>256||!Valid(items,count*4))return false;
 for(unsigned i=0;i<count;++i)if(REX_LOAD_U32(items+i*4)==target){
  const auto copy=rex::Runtime::instance()->memory()->SystemHeapAlloc(972+sizeof(name),16);
  if(!copy)return false;
  std::memcpy(base+copy,base+source,972);
  std::memcpy(base+copy+972,name,sizeof(name));
  REX_STORE_U32(copy,109);REX_STORE_U32(copy+4,copy+972);
  REX_STORE_U32(copy+68,0);REX_STORE_U32(copy+20,0);
  REX_STORE_U32(items+i*4,copy);
  return true;
 }
 return false;
}
bool PrepareJoannaDressHead(uint8_t* base){
 static std::mutex mutex;
 std::lock_guard lock(mutex);
 constexpr char name[]="aid_darkcharacter_xenonheads_jo_shortdress_head";
 std::error_code error;
 if(!std::filesystem::is_regular_file(utils::RepoRoot()/"assets"/"pdz"/name/"default.dbm",error)||error)return false;
 const auto source=Definition(base,157,true),target=Definition(base,119,true);
 if(!Valid(source,512)||!Valid(target,16))return false;
 const auto currentName=REX_LOAD_U32(target+4);
 if(Valid(currentName,sizeof(name))&&!std::memcmp(base+currentName,name,sizeof(name)))return true;
 // Head 119 is the absent Virtual Trooper placeholder; that character uses 67.
 // Isolate this outfit's tattoo from the shared Joanna head.
 const auto defs=REX_LOAD_U32(0x82D6BFE0);
 if(!Valid(defs,16))return false;
 const auto vec=REX_LOAD_U32(defs+12);
 if(!Valid(vec,12))return false;
 const auto items=REX_LOAD_U32(vec),count=REX_LOAD_U32(vec+8);
 if(count>256||!Valid(items,count*4))return false;
 for(unsigned i=0;i<count;++i)if(REX_LOAD_U32(items+i*4)==target){
  const auto copy=rex::Runtime::instance()->memory()->SystemHeapAlloc(512+sizeof(name),16);
  if(!copy)return false;
  std::memcpy(base+copy,base+source,512);
  std::memcpy(base+copy+512,name,sizeof(name));
  REX_STORE_U32(copy,119);REX_STORE_U32(copy+4,copy+512);
  REX_STORE_U32(copy+12,0);
  REX_STORE_U32(items+i*4,copy);
  return true;
 }
 return false;
}
const pdz_mp::Character* Prepare(uint8_t* base,unsigned choice,unsigned headChoice=0,unsigned nativeBody=0,bool independent=false,unsigned nativeHead=0){
 if(!choice&&headChoice){const auto& list=pdz_mp::Characters();for(unsigned i=0;i<list.size();++i)if(list[i].body==nativeBody){choice=i+1;break;}}
 if(!choice||choice>pdz_mp::Characters().size())return nullptr;
 thread_local pdz_mp::Character combined;
 combined=pdz_mp::Characters()[choice-1];
 if(headChoice&&headChoice<=pdz_mp::Characters().size())combined.head=pdz_mp::Characters()[headChoice-1].head;
 else if(independent)combined.head=nativeHead;
 const auto& c=combined;
 if(c.head==119&&!PrepareJoannaDressHead(base))return nullptr;
 if(c.body==109&&(!PrepareJoannaDress(base)||!PrepareJoannaDressHead(base)))return nullptr;
 auto b=Definition(base,c.body),h=Definition(base,c.head,true),hands=Definition(base,c.hands);
 if(!b||!h||!hands)return nullptr;
 auto handName=REX_LOAD_U32(b+16);
 if(c.body==80||!Valid(handName,2)||base[handName]=='_')handName=REX_LOAD_U32(hands+16);
 if(c.body==80||c.body==82||c.body==109){
  const auto bareHands=DressHandsName(base,c.body==82);
  if(!bareHands)return nullptr;
  handName=bareHands;
 }
 // Campaign definitions exist in MP even when their model packages do not.
 // The native loader dereferences the body and ragdoll without a null check.
 // Keep the original character unless every required model is already loaded
 // or available as an extracted loose resource in this installation.
 const auto available=[&](uint32_t name,uint32_t resource){
  if(Valid(resource,12))return true;
  std::string asset;
  for(unsigned i=0;i<192;++i){
   if(!Valid(name+i,1))return false;
   const char ch=char(base[name+i]);if(!ch)break;
   // Retail Dr. Carroll hands are named ..._dr.carrol_hands. A period
   // inside an asset name is valid; separators remain forbidden.
   if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='.'))return false;
   asset+=ch;
  }
  if(!asset.starts_with("aid_darkcharacter_")||asset.size()>=192||asset.find("..")!=std::string::npos||asset.back()=='.')return false;
  std::error_code error;
  const auto file=utils::RepoRoot()/"assets"/"pdz"/asset/"default.dbm";
  return std::filesystem::is_regular_file(file,error)&&std::filesystem::file_size(file,error)>384&&!error;
 };
 if(!available(REX_LOAD_U32(b+4),REX_LOAD_U32(b+68))||
    !available(REX_LOAD_U32(b+32),REX_LOAD_U32(b+36))||
    !available(handName,REX_LOAD_U32(b+20))||
    !available(REX_LOAD_U32(h+4),REX_LOAD_U32(h+12))||
    !available(REX_LOAD_U32(b+44),REX_LOAD_U32(b+48))||
    !available(REX_LOAD_U32(b+52),REX_LOAD_U32(b+56))){
  static std::set<unsigned> warned;
  if(warned.insert(choice).second)REXLOG_WARN("MP character {} unavailable: required character assets missing; retaining native appearance",choice);
  return nullptr;
 }
 REX_STORE_U32(b+16,handName);
 return &c;
}
}
// Loose CAFF models use a direct file reader, unlike the package reader's
// relative-path probe. Give both readers the same fully qualified guest path.
REX_EXTERN(__imp__sub_821F5E58);
REX_HOOK_RAW(sub_821F5E58){
 const auto output=ctx.r4.u32;
 __imp__sub_821F5E58(ctx,base);
 if(!Valid(output,260))return;
 constexpr char prefix[]="aid_darkcharacter_";
 if(std::memcmp(base+output,prefix,sizeof(prefix)-1)!=0)return;
 const auto end=static_cast<const uint8_t*>(std::memchr(base+output,0,252));
 if(!end)return;
 const auto length=size_t(end-(base+output));
 const auto klobbLength=sizeof(pdz_klobb::kModelName)-1;
 const bool klobbPath=pdz_klobb::LoadingModel()&&length>=klobbLength&&
     !std::memcmp(base+output,pdz_klobb::kModelName,klobbLength)&&
     (length==klobbLength||base[output+klobbLength]=='\\'||base[output+klobbLength]=='/');
 if(!Multiplayer(base)&&!NightclubCampaign(base)&&!klobbPath)return;
 // The native reader recognizes the retail drive prefix. A named drive such
 // as klobb: is treated as relative and becomes D:\PDZ\klobb:\..., which
 // prevents the pickup from loading its model. The installer supplies this
 // exact model under the retail asset root too.
 constexpr char mount[]="D:\\PDZ\\";
 std::memmove(base+output+7,base+output,length+1);
 std::memcpy(base+output,mount,7);
}
REX_EXTERN(__imp__sub_8251C358);
REX_HOOK_RAW(sub_8251C358){
 const auto owner=ctx.r3.u32;unsigned slot=32,choice=0;
 if(owner&&Multiplayer(base)){
  auto owners=Owners(base);auto it=std::find(owners.begin(),owners.end(),owner);
  if(it!=owners.end()){
   slot=unsigned(it-owners.begin());choice=Selected(ctx,base,slot,owner,0);
   if(auto c=Prepare(base,choice,pdz_mp::HeadChoice(slot),ctx.r4.u32,pdz_mp::IndependentParts(slot),ctx.r5.u32)){ctx.r4.u64=c->body;ctx.r5.u64=c->head;}
   else choice=0;
  }
 }
 __imp__sub_8251C358(ctx,base);
 if(slot<32&&ctx.r3.u32){applied[slot]={owner,ctx.r3.u32,choice};
  if(choice)REXLOG_INFO("MP character spawn slot {} choice {} actor {:08X}",slot,choice,ctx.r3.u32);}
}

// Use native resource loading, rig rebinding and cleanup. Override only the
// updater's two definition lookups on this thread, without changing faction tables.
REX_EXTERN(__imp__sub_82278788);
REX_HOOK_RAW(sub_82278788){
  if(refreshCharacter&&ctx.lr==0x825B4730)ctx.r3.u64=refreshCharacter->body;
 else if(NightclubCampaign(base)&&(ctx.r3.u32==3||ctx.r3.u32==6||ctx.r3.u32==85||ctx.r3.u32==90||ctx.r3.u32==110)){
  const auto& list=pdz_mp::Characters();
  auto it=std::find_if(list.begin(),list.end(),[](const auto& c){return c.body==109;});
  if(it!=list.end())if(auto c=Prepare(base,unsigned(it-list.begin())+1))ctx.r3.u64=c->body;
 }
 __imp__sub_82278788(ctx,base);
}
REX_EXTERN(__imp__sub_822787E0);
REX_HOOK_RAW(sub_822787E0){
  if(refreshCharacter&&ctx.lr==0x825B4740)ctx.r3.u64=refreshCharacter->head;
 else if(NightclubCampaign(base)&&(ctx.r3.u32==101||ctx.r3.u32==120||ctx.r3.u32==125||ctx.r3.u32==157)){
  if(PrepareJoannaDressHead(base))ctx.r3.u64=119;
 }
 __imp__sub_822787E0(ctx,base);
}
REX_EXTERN(__imp__sub_825B45D0);
REX_HOOK_RAW(sub_825B45D0){
 const auto actor=ctx.r3.u32;
 if(Multiplayer(base)&&Valid(actor,6564)){
  const auto owners=Owners(base);
  // Bot actors keep an AI controller at +6560, not their roster owner.
  auto it=std::find_if(owners.begin(),owners.end(),[&](uint32_t o){return REX_LOAD_U32(o+964)==actor;});
  if(it!=owners.end()){
   const unsigned slot=unsigned(it-owners.begin()),choice=Selected(ctx,base,slot,*it,actor);
   if(auto c=Prepare(base,choice,pdz_mp::HeadChoice(slot),REX_LOAD_U32(actor+5280),pdz_mp::IndependentParts(slot),REX_LOAD_U32(actor+5284))){
    if(REX_LOAD_U32(actor+5280)!=c->body||REX_LOAD_U32(actor+5284)!=c->head){
     const auto previousBody=REX_LOAD_U32(actor+5280),previousHead=REX_LOAD_U32(actor+5284);
     REX_STORE_U32(actor+5280,0xffffffffu);
     struct RefreshScope {const pdz_mp::Character* saved;RefreshScope(const pdz_mp::Character* p):saved(refreshCharacter){refreshCharacter=p;}~RefreshScope(){refreshCharacter=saved;}} scope(c);
     __imp__sub_825B45D0(ctx,base);
     const bool bound=REX_LOAD_U32(actor+5264)==Definition(base,c->body)&&REX_LOAD_U32(actor+5268)==Definition(base,c->head,true);
     REX_STORE_U32(actor+5280,bound?c->body:previousBody);REX_STORE_U32(actor+5284,bound?c->head:previousHead);
     if(bound)REXLOG_INFO("MP character applied slot {} body {} head {}",slot,c->body,c->head);
    }
    applied[slot]={*it,actor,choice};return;
   }
  }
 }
 __imp__sub_825B45D0(ctx,base);
}





