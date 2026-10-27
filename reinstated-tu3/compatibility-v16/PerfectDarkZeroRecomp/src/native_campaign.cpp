#include "perfect_dark_zero_mode.h"
#include "pd_menu_state.h"
#include "mission_summary_state.h"
#include "cheat_progress.h"
#include "debug_position.h"
#include "weapon_challenge.h"
#include "mp_characters.h"
#include "update_service.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "campaign_menu.h"
#include "game_cvars.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include "coop_ally.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/system/xmemory.h>
#include <array>
#include <atomic>
#include <string>
#include <map>
#include <chrono>
#include <algorithm>
#include <bit>
#include <cmath>

REX_EXTERN(__imp__sub_8278CCC8);
REX_EXTERN(__imp__sub_82334C38);
REX_EXTERN(__imp__sub_8242C438);
REX_EXTERN(__imp__sub_8278CC40);
REX_EXTERN(__imp__sub_8222A9D0);
REX_EXTERN(__imp__sub_82229E68);
namespace {
std::atomic<bool> choosing{false}, cancel{false};
uint32_t scene = 0, mission_control = 0;
std::array<uint32_t, 4> controls{};
std::array<std::u16string, 4> original_text;
struct HelpText { std::u16string original, displayed; };
std::map<uint32_t, HelpText> help_text;
std::map<uint32_t, HelpText> portrait_paths;
unsigned selected = 0;
bool busy = false;
uint32_t solo_handle=0, bot_coop_handle=0, local_coop_handle=0;
bool routing_bot_coop=false;


bool Valid(uint32_t p, uint32_t length) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !p || uint64_t(p) + length > 0x100000000ull) return false;
  for (uint64_t a = p; a < uint64_t(p) + length;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(a));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(a), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    a = (a / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
std::u16string ReadText(uint32_t p, uint8_t* base) {
  std::u16string value;
  for (unsigned i = 0; i < 512 && Valid(p + i * 2, 2); ++i) {
    auto c = REX_LOAD_U16(p + i * 2);
    if (!c) break;
    value += char16_t(c);
  }
  return value;
}
void WriteText(uint32_t p, std::u16string_view text, uint8_t* base) {
  for (unsigned i = 0; i < text.size(); ++i) REX_STORE_U16(p + i * 2, text[i]);
  REX_STORE_U16(p + text.size() * 2, 0);
}
uint32_t Element(PPCContext& call, uint32_t handle, uint8_t* base) {
  call.r3.u64 = handle; call.r4.u64 = REX_LOAD_U32(0x82D356C4);
  sub_82779A50(call, base);
  return call.r3.u32;
}
uint32_t Child(PPCContext& call, uint32_t parent, std::u16string_view id, uint8_t* base) {
  const uint32_t out = call.r1.u32 + 80, text = call.r1.u32 + 128;
  WriteText(text, id, base); REX_STORE_U32(out, 0);
  call.r3.u64 = parent; call.r4.u64 = text; call.r5.u64 = out;
  sub_8277ADB8(call, base);
  return REX_LOAD_U32(out);
}
std::u16string GetText(PPCContext& call, uint32_t handle, uint8_t* base,
                       uint32_t property = 0x82001184) {
  call.r3.u64 = handle; call.r4.u64 = property;
  sub_823AE638(call, base);
  return ReadText(call.r3.u32, base);
}
void SetText(PPCContext& call, uint32_t handle, std::u16string_view text, uint8_t* base,
             uint32_t property = 0x82001184) {
  WriteText(call.r1.u32 + 256, text, base);
  call.r3.u64 = handle; call.r4.u64 = property; call.r5.u64 = call.r1.u32 + 256;
  sub_823AEA20(call, base); // Native property setter owns/copies the UTF-16 string.
}
// The updater is a fourth native button beneath Options.
auto& updatePage=pdz_menu::page;
uint32_t updateEntry=0;
uint32_t exitEntry=0;
auto& updateBack=pdz_menu::back;
std::array<uint32_t,4> updateControls{};
std::array<std::u16string,4> updateOriginal{};
uint32_t originalOptionsControl=0;
std::vector<std::u16string> changelogPages;
size_t changelogPage=0;
void UpdateNavigation(PPCContext& call,uint8_t* base,uint32_t handle,unsigned offset,std::u16string_view target){
  call.r3.u64=handle;sub_8276C0F8(call,base);handle=call.r3.u32;
  call.r3.u64=handle;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_82779A50(call,base);
  const auto control=call.r3.u32;if(!Valid(control,40))return;
  if(ReadText(REX_LOAD_U32(control+offset),base)==target)return;
  WriteText(call.r1.u32+256,target,base);
  // Use XUI's owning string assignment on the actual Control navigation slot.
  // The derived Options class does not resolve these named properties.
  call.r3.u64=control+offset;call.r4.u64=call.r1.u32+256;sub_8276C2F8(call,base);
}
#include "native_mp_menu.inl"
void EnsureUpdateEntry(PPCContext& call,uint8_t* base,uint32_t options){
  // Render callbacks supply the Element alias, which cannot look up derived
  // Control state. Resolve the complete object before reading its navigation.
  call.r3.u64=options;sub_8276C0F8(call,base);options=call.r3.u32;
  const auto e=Element(call,options,base);if(!Valid(e,240))return;
  const auto parent=REX_LOAD_U32(e+40);if(!Valid(parent,44))return;
  const auto existing=Element(call,updateEntry,base);
  if(Valid(existing,240)&&REX_LOAD_U32(existing+40)==parent){
    UpdateNavigation(call,base,options,32,u"CheckForUpdates");return;
  }
  updateEntry=0;
  call.r3.u64=options;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_82779A50(call,base);
  const auto visual=Valid(call.r3.u32,12)?ReadText(REX_LOAD_U32(call.r3.u32+8),base):u"DarkButton";
  const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
  call.r3.u64=0x820ACFFC;call.r4.u64=out;sub_82776858(call,base);
  if(call.r3.s32<0)return;
  const auto h=REX_LOAD_U32(out);if(!h)return;
  SetText(call,h,u"CheckForUpdates",base,0x820AB378);
  SetText(call,h,u"CHECK FOR UPDATES",base);
  
  call.r3.u64=REX_LOAD_U32(parent);call.r4.u64=h;sub_8277A658(call,base);
  if(call.r3.s32<0){call.r3.u64=h;sub_827768C8(call,base);return;}
  call.r3.u64=call.r1.u32+96;call.r4.u64=41;sub_82789C88(call,base);
  call.r3.u64=h;call.r4.u64=call.r1.u32+96;sub_82776920(call,base);
  WriteText(call.r1.u32+256,visual.empty()?u"DarkButton":visual,base);
  call.r3.u64=h;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_8276C050(call,base);
  const auto controlObject=call.r3.u32;
  REX_STORE_U32(call.r1.u32+112,5);REX_STORE_U32(call.r1.u32+116,call.r1.u32+256);
  call.r3.u64=0;call.r4.u64=controlObject;call.r5.u64=call.r1.u32+112;sub_8278D7C0(call,base);
  call.r3.u64=h;call.f1.f64=std::bit_cast<float>(REX_LOAD_U32(e+12));
  call.f2.f64=std::bit_cast<float>(REX_LOAD_U32(e+16));sub_8277E5F0(call,base);
  const auto v=call.r1.u32+96;
  REX_STORE_U32(v,REX_LOAD_U32(e+44));
  REX_STORE_U32(v+4,std::bit_cast<uint32_t>(std::bit_cast<float>(REX_LOAD_U32(e+48))+72.f));
  REX_STORE_U32(v+8,REX_LOAD_U32(e+52));
  call.r3.u64=h;call.r4.u64=v;sub_8277AA88(call,base);
  UpdateNavigation(call,base,h,28,u"Options");
  UpdateNavigation(call,base,h,32,u"MissionMode");
  UpdateNavigation(call,base,options,32,u"CheckForUpdates");
  updateEntry=h;
  REXLOG_INFO("PDZ updater main-menu button created, visual length {}",visual.size());
}
// The native pause scene stays alive while the shared settings panel is open.
uint32_t pauseExtendedEntry=0;
bool extendedFromPause=false;
uint32_t PauseMenuOwner(uint32_t element,uint8_t* base){
  for(unsigned depth=0;element&&depth<64&&Valid(element,44);++depth){
    const auto id=ReadText(REX_LOAD_U32(element+4),base);
    if(id.starts_with(u"aid_darkxui_ingame_")&&id.find(u"start")!=std::u16string::npos)return element;
    element=REX_LOAD_U32(element+40);
  }
  return 0;
}
void EnsurePauseExtendedEntry(PPCContext& call,uint8_t* base,uint32_t options){
  // Render callbacks supply the Element alias, which cannot look up derived
  // Control state. Resolve the complete object before reading its navigation.
  call.r3.u64=options;sub_8276C0F8(call,base);options=call.r3.u32;
  const auto e=Element(call,options,base);if(!Valid(e,240))return;
  const auto parent=REX_LOAD_U32(e+40);if(!Valid(parent,44))return;
  const auto existing=Element(call,pauseExtendedEntry,base);
  if(Valid(existing,240)&&REX_LOAD_U32(existing+40)==parent){
    UpdateNavigation(call,base,options,32,u"PauseExtendedOptions");return;
  }
  pauseExtendedEntry=0;
  call.r3.u64=options;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_82779A50(call,base);
  const auto visual=Valid(call.r3.u32,12)?ReadText(REX_LOAD_U32(call.r3.u32+8),base):u"DarkButton";
  const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
  call.r3.u64=0x820ACFFC;call.r4.u64=out;sub_82776858(call,base);
  if(call.r3.s32<0)return;
  const auto h=REX_LOAD_U32(out);if(!h)return;
  SetText(call,h,u"PauseExtendedOptions",base,0x820AB378);
  SetText(call,h,u"EXTENDED OPTIONS",base);
  
  call.r3.u64=REX_LOAD_U32(parent);call.r4.u64=h;sub_8277A658(call,base);
  if(call.r3.s32<0){call.r3.u64=h;sub_827768C8(call,base);return;}
  call.r3.u64=call.r1.u32+96;call.r4.u64=41;sub_82789C88(call,base);
  call.r3.u64=h;call.r4.u64=call.r1.u32+96;sub_82776920(call,base);
  WriteText(call.r1.u32+256,visual.empty()?u"DarkButton":visual,base);
  call.r3.u64=h;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_8276C050(call,base);
  const auto controlObject=call.r3.u32;
  REX_STORE_U32(call.r1.u32+112,5);REX_STORE_U32(call.r1.u32+116,call.r1.u32+256);
  call.r3.u64=0;call.r4.u64=controlObject;call.r5.u64=call.r1.u32+112;sub_8278D7C0(call,base);
  call.r3.u64=h;call.f1.f64=std::bit_cast<float>(REX_LOAD_U32(e+12));
  call.f2.f64=std::bit_cast<float>(REX_LOAD_U32(e+16));sub_8277E5F0(call,base);
  const auto v=call.r1.u32+96;
  REX_STORE_U32(v,REX_LOAD_U32(e+44));
  REX_STORE_U32(v+4,std::bit_cast<uint32_t>(std::bit_cast<float>(REX_LOAD_U32(e+48))+55.6f));
  REX_STORE_U32(v+8,REX_LOAD_U32(e+52));
  call.r3.u64=h;call.r4.u64=v;sub_8277AA88(call,base);
  UpdateNavigation(call,base,h,28,u"buttonAbandon");
  UpdateNavigation(call,base,h,32,u"buttonControls");
  UpdateNavigation(call,base,options,32,u"PauseExtendedOptions");
  const auto first=Child(call,REX_LOAD_U32(parent),u"buttonControls",base);
  if(first)UpdateNavigation(call,base,first,28,u"PauseExtendedOptions");
  pauseExtendedEntry=h;
  REXLOG_INFO("PDZ pause Extended Options button created, visual length {}",visual.size());
}
void EnsureExitEntry(PPCContext& call,uint8_t* base){
  const auto e=Element(call,updateEntry,base);if(!Valid(e,240))return;
  const auto parent=REX_LOAD_U32(e+40);if(!Valid(parent,4))return;
  const auto existing=Element(call,exitEntry,base);
  if(Valid(existing,240)&&REX_LOAD_U32(existing+40)==parent)return;
  exitEntry=0;
  const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
  call.r3.u64=0x820ACFFC;call.r4.u64=out;sub_82776858(call,base);
  if(call.r3.s32<0)return;
  const auto h=REX_LOAD_U32(out);if(!h)return;
  SetText(call,h,u"ExitGame",base,0x820AB378);SetText(call,h,u"EXIT",base);
  call.r3.u64=REX_LOAD_U32(parent);call.r4.u64=h;sub_8277A658(call,base);
  if(call.r3.s32<0){call.r3.u64=h;sub_827768C8(call,base);return;}
  call.r3.u64=call.r1.u32+96;call.r4.u64=41;sub_82789C88(call,base);
  call.r3.u64=h;call.r4.u64=call.r1.u32+96;sub_82776920(call,base);
  WriteText(call.r1.u32+256,u"DarkButton",base);
  call.r3.u64=h;call.r4.u64=REX_LOAD_U32(0x82D356F8);sub_8276C050(call,base);
  const auto control=call.r3.u32;
  REX_STORE_U32(call.r1.u32+112,5);REX_STORE_U32(call.r1.u32+116,call.r1.u32+256);
  call.r3.u64=0;call.r4.u64=control;call.r5.u64=call.r1.u32+112;sub_8278D7C0(call,base);
  call.r3.u64=h;call.f1.f64=75.f;call.f2.f64=28.f;sub_8277E5F0(call,base);
  const auto v=call.r1.u32+96;
  REX_STORE_U32(v,std::bit_cast<uint32_t>(std::bit_cast<float>(REX_LOAD_U32(e+44))-20.f));
  REX_STORE_U32(v+4,std::bit_cast<uint32_t>(std::bit_cast<float>(REX_LOAD_U32(e+48))+62.f));
  REX_STORE_U32(v+8,REX_LOAD_U32(e+52));
  call.r3.u64=h;call.r4.u64=v;sub_8277AA88(call,base);
  UpdateNavigation(call,base,h,28,u"CheckForUpdates");
  UpdateNavigation(call,base,h,32,u"MissionMode");
  exitEntry=h;
}
void ShowUpdateEntry(PPCContext& call,uint8_t* base,bool show){
  if(!updateEntry)return;
  const auto element=Element(call,updateEntry,base);if(!Valid(element,44))return;
  const auto parent=REX_LOAD_U32(element+40);if(!Valid(parent,4))return;
  const auto parentHandle=REX_LOAD_U32(parent);
  call.r3.u64=updateEntry;call.r4.u64=show;sub_8277A5B8(call,base);
  call.r3.u64=updateEntry;call.r4.u64=show;sub_8278ABF8(call,base); // XUI visibility, not just enabled state.
  if(exitEntry&&Element(call,exitEntry,base)){
    call.r3.u64=exitEntry;call.r4.u64=show;sub_8277A5B8(call,base);
    call.r3.u64=exitEntry;call.r4.u64=show;sub_8278ABF8(call,base);
    UpdateNavigation(call,base,updateEntry,32,u"ExitGame");
  }
  const auto options=Child(call,parentHandle,u"Options",base);
  const auto missions=Child(call,parentHandle,u"MissionMode",base);
  if(options)UpdateNavigation(call,base,options,32,show?u"CheckForUpdates":u"MissionMode");
  // Close the loop in both directions, restoring the three-button loop when hidden.
  if(missions)UpdateNavigation(call,base,missions,28,show?(exitEntry?u"ExitGame":u"CheckForUpdates"):u"Options");
}
std::u16string UpdateText(std::string_view text){
  std::u16string out;for(unsigned char c:text)out+=c>=32&&c<127?char16_t(c):u' ';
  return out.substr(0,220);
}
void UpdateLabels(PPCContext& call,uint8_t* base){
  if(pdz_menu::Overlay())return;
  if(updatePage==1){
    UpdateNavigation(call,base,updateControls[0],28,u"combat arena");
    UpdateNavigation(call,base,updateControls[0],32,u"combat arena");
    UpdateNavigation(call,base,updateControls[1],28,u"MissionMode");
    UpdateNavigation(call,base,updateControls[1],32,u"MissionMode");
  }
  const auto state=pdz_update::Poll();
  const std::array<std::u16string,4> labels=updatePage==3?
    std::array<std::u16string,4>{u"PREVIOUS PAGE",u"NEXT PAGE",u"RELOAD",u"CHANGELOGS"}:updatePage==1?
    std::array<std::u16string,4>{u"GAME OPTIONS",u"EXTENDED OPTIONS",u"",u"OPTIONS"}:
    std::array<std::u16string,4>{state.phase=="ready"?u"RESTART NOW":state.phase=="available"?u"DOWNLOAD UPDATE":u"CHECK AGAIN",
      state.phase=="ready"?u"WAIT":u"REDOWNLOAD",u"CHANGELOGS",u"CHECK FOR UPDATES"};
  for(unsigned i=0;i<4;++i)if(GetText(call,updateControls[i],base)!=labels[i])SetText(call,updateControls[i],labels[i],base);
}
void CloseUpdates(PPCContext& call,uint8_t* base){
  pdz_update::Wait();updatePage=0;updateBack=false;
  for(unsigned i=0;i<4;++i)if(Element(call,updateControls[i],base))SetText(call,updateControls[i],updateOriginal[i],base);
  UpdateNavigation(call,base,updateControls[0],32,u"combat arena");
  UpdateNavigation(call,base,updateControls[1],28,u"MissionMode");
  UpdateNavigation(call,base,updateControls[1],32,u"Options");
  ShowUpdateEntry(call,base,true);
  call.r3.u64=updateControls[2];call.r4.u64=255;sub_8277F5C8(call,base);
}
void BackUpdates(PPCContext& call,uint8_t* base){
  if(updatePage==pdz_menu::Progression||updatePage==pdz_menu::Cheats){
    updatePage=pdz_menu::Closed;updateBack=false;return;
  }
  if(extendedFromPause){
    extendedFromPause=false;updatePage=pdz_menu::Closed;updateBack=false;
    if(Element(call,pauseExtendedEntry,base)){
      call.r3.u64=pauseExtendedEntry;call.r4.u64=255;sub_8277F5C8(call,base);
    }
    return;
  }
  if(updatePage==4){updatePage=1;UpdateLabels(call,base);return;}
  if(updatePage!=3){CloseUpdates(call,base);return;}
  updatePage=2;UpdateLabels(call,base);
  call.r3.u64=updateControls[2];call.r4.u64=255;sub_8277F5C8(call,base);
}
bool UpdatePress(PPCContext& call,uint8_t* base,uint32_t control,uint32_t message,uint32_t handle,std::u16string_view id){
  if(!updatePage){
    if(id==u"PauseExtendedOptions"&&PauseMenuOwner(Element(call,handle,base),base)){
      extendedFromPause=true;updateBack=false;updatePage=pdz_menu::Extended;return true;
    }
    if(id==u"ExitGame"&&!choosing){
      if(auto* rt=rex::Runtime::instance())if(auto* window=rt->display_window()){
        window->app_context().CallInUIThreadDeferred([]{
          if(auto* runtime=rex::Runtime::instance())if(auto* current=runtime->display_window())current->RequestClose();
        });
      }
      return true;
    }
    if((id!=u"CheckForUpdates"&&id!=u"Options")||choosing)return false;
    const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
    call.r3.u64=handle;call.r4.u64=out;sub_8277A6F0(call,base);
    const auto parent=REX_LOAD_U32(out);
    std::array<uint32_t,4> found{Child(call,parent,u"MissionMode",base),Child(call,parent,u"combat arena",base),Child(call,parent,u"Options",base),Child(call,parent,u"header1",base)};
    for(auto& h:found){if(!h||!Element(call,h,base))return false;call.r3.u64=h;sub_8276C0F8(call,base);h=call.r3.u32;}
    updateControls=found;originalOptionsControl=control;
    for(unsigned i=0;i<4;++i)updateOriginal[i]=GetText(call,found[i],base);
    updatePage=id==u"Options"?1:2;if(updatePage==2)pdz_update::Check();ShowUpdateEntry(call,base,false);UpdateLabels(call,base);
    call.r3.u64=found[0];call.r4.u64=255;sub_8277F5C8(call,base);return true;
  }
  if(pdz_menu::Overlay())return true;
  const auto back=[&]{BackUpdates(call,base);};
  if(id==u"BackButton"){back();return true;}
  for(unsigned i=0;i<3;++i)if(handle==updateControls[i]){
    if(updatePage==3){
      if(i==0&&changelogPage) --changelogPage;
      else if(i==1&&changelogPage+1<changelogPages.size()) ++changelogPage;
      else if(i==2){changelogPages=pdz_update::ReadChangelog();changelogPage=0;}
    }else if(updatePage==1){
      if(i==0){CloseUpdates(call,base);call.r3.u64=originalOptionsControl;call.r4.u64=message;__imp__sub_8278CCC8(call,base);}
      else if(i==1){updatePage=4;UpdateLabels(call,base);}
      else back();
    }else{
      const auto state=pdz_update::Poll();
      if(i==0){if(state.phase=="ready")pdz_update::Restart();else if(state.phase=="available")pdz_update::Download();else pdz_update::Check();}
      else if(i==1&&state.phase=="ready")pdz_update::Wait();
      else if(i==1)pdz_update::Redownload();
      else {
        pdz_update::Wait();changelogPages=pdz_update::ReadChangelog();changelogPage=0;updatePage=3;
      }
      if(updatePage)UpdateLabels(call,base);
    }
    return true;
  }
  return false;
}
// Refresh only the frozen player's projection, never the simulation or camera
// animation. This follows the projection tail of retail sub_82205170.
void PreviewPausedFov(PPCContext& call,uint8_t* base){
  static uint32_t lastCamera=0;
  static float lastSetting=58.f;
  const float setting=static_cast<float>(REXCVAR_GET(pdz_fov));
  if(!std::isfinite(setting)||setting<40.f||setting>120.f)return;
  const uint32_t camera=REX_LOAD_U32(0x82D68320);
  const uint32_t session=REX_LOAD_U32(0x82D6B2B0);
  const bool paused=Valid(session,1)&&REX_LOAD_U8(session)!=0;
  if(!paused||!extendedFromPause||camera!=lastCamera){
    lastCamera=camera;lastSetting=setting;return;
  }
  if(setting==lastSetting||!Valid(camera,0x444))return;
  const uint32_t renderCamera=REX_LOAD_U32(camera+60);
  if(!Valid(renderCamera,496)||!Valid(REX_LOAD_U32(renderCamera+16),144))return;
  const auto readFloat=[&](uint32_t p){return std::bit_cast<float>(REX_LOAD_U32(p));};
  // Retain the zoom ratio of the paused view instead of forcing an unzoomed FOV.
  const float fov=readFloat(camera+1088)*setting/lastSetting;
  if(!std::isfinite(fov)||fov<=0.f||fov>=180.f)return;
  const float half=readFloat(0x820EC130);
  call.f1.f64=fov*half;call.f2.f64=readFloat(0x820ECEEC);
  sub_82644AF0(call,base);
  const float projection=float(call.f1.f64)*readFloat(0x820EC134)*half;
  if(!std::isfinite(projection)||projection<=0.f)return;
  REX_STORE_U32(camera+1088,std::bit_cast<uint32_t>(fov));
  REX_STORE_U32(renderCamera+68,std::bit_cast<uint32_t>(projection));
  REX_STORE_U8(renderCamera+84,1);
  call.r3.u64=renderCamera;call.r4.u64=camera+1024;
  sub_82643740(call,base);
  lastSetting=setting;
}
void UpdateTick(PPCContext& ctx,uint8_t* base,uint32_t rendered){
  if(busy)return;
  auto call=ctx;call.r1.u32-=1536;if(!Valid(call.r1.u32,1536))return;
  if(updatePage==pdz_menu::Progression||updatePage==pdz_menu::Cheats){
    const int challenge=pdz_menu::challenge_request.exchange(-1);
    if(challenge>=0)pdz_challenge::Select(unsigned(challenge));
    if(updateBack.exchange(false))BackUpdates(call,base);
    return;
  }
  PreviewPausedFov(call,base);
  const auto element=Element(call,rendered,base);if(!Valid(element,44))return;
  const auto id=ReadText(REX_LOAD_U32(element+4),base);
  if(id==u"buttonAbandon"&&!updatePage&&PauseMenuOwner(element,base)){
    busy=true;EnsurePauseExtendedEntry(call,base,rendered);busy=false;
  }
  if(id==u"Options"&&!updatePage&&!choosing){
    busy=true;EnsureUpdateEntry(call,base,rendered);EnsureExitEntry(call,base);ShowUpdateEntry(call,base,true);busy=false;
  }
  if(choosing&&updateEntry){busy=true;ShowUpdateEntry(call,base,false);busy=false;}
  static auto mainSeen=std::chrono::steady_clock::time_point{};
  if(id==u"MissionMode"&&!choosing)mainSeen=std::chrono::steady_clock::now();
  if(!updatePage&&((id!=u"location text"&&id!=u"level name")||choosing||std::chrono::steady_clock::now()-mainSeen>std::chrono::milliseconds(100)))return;
  busy=true;
  if(updatePage&&updateBack.exchange(false))BackUpdates(call,base);
  if(updatePage)UpdateLabels(call,base);
  if(pdz_menu::Overlay()){busy=false;return;}
  if(id==u"level name"||id==u"location text"){
    call.r3.u64=rendered;sub_8276C0F8(call,base);const auto handle=call.r3.u32;
    if(updatePage){
      const auto s=pdz_update::Poll();
      auto text=id==u"level name"?(updatePage==1?u"OPTIONS":u"CHECK FOR UPDATES"):updatePage==1?u"Choose Game Options or Extended Options.":UpdateText(s.message);
      if(updatePage==3){
        text=id==u"level name"?u"CHANGELOGS "+UpdateText(std::to_string(changelogPage+1))+u" / "+UpdateText(std::to_string(changelogPages.size())):
          changelogPages.empty()?u"No release notes yet.":changelogPages[changelogPage];
      }
      if(updatePage==2&&id==u"location text"){
        if(s.phase=="downloading")text+=u" "+UpdateText(std::to_string(s.percent))+u"%";
        if(s.seconds>=0)text+=u" Restarting in "+UpdateText(std::to_string(s.seconds))+u" seconds.";
      }
      if(GetText(call,handle,base)!=text)SetText(call,handle,text,base);
    }else {
      call.r3.u64=255;sub_82779D88(call,base);
      static std::map<uint32_t,HelpText> entryHelp;
      const auto current=GetText(call,handle,base);
      // Query focus again because GetText changes the guest call registers.
      call.r3.u64=255;sub_82779D88(call,base);
      if(call.r3.u32==updateEntry||call.r3.u32==exitEntry){
        const bool exitFocused=call.r3.u32==exitEntry;
        auto& saved=entryHelp[handle];
        if(current!=saved.displayed)saved.original=current;
        saved.displayed=exitFocused?(id==u"level name"?u"EXIT":u"Exit the game and return to the desktop."):
          (id==u"level name"?u"CHECK FOR UPDATES":u"Check for updates or redownload the latest release.");
        if(current!=saved.displayed)SetText(call,handle,saved.displayed,base);
      }else if(auto old=entryHelp.find(handle);old!=entryHelp.end()){
        if(current==old->second.displayed)SetText(call,handle,old->second.original,base);
        entryHelp.erase(old);
      }
    }
    if(!updatePage&&id==u"location text"&&pdz_update::Successful()){
      static auto shown=std::chrono::steady_clock::now();
      if(std::chrono::steady_clock::now()-shown<std::chrono::seconds(10))SetText(call,handle,u"Update successful",base);
      else pdz_update::Acknowledge();
    }
  }
  busy=false;
}
void BotCoopMenu(PPCContext& ctx,uint8_t* base,uint32_t rendered_handle) {
  static bool updating=false;
  if(updating)return;
  auto call=ctx;call.r1.u32-=1536;
  if(!Valid(call.r1.u32,1536))return;
  const auto element=Element(call,rendered_handle,base);
  if(!Valid(element,44))return;
  const auto id=ReadText(REX_LOAD_U32(element+4),base);
  call.r3.u64=rendered_handle;sub_8276C0F8(call,base);
  const auto handle=call.r3.u32;
  if(id==u"Solo")solo_handle=handle;
  if(id==u"buttonLocal")local_coop_handle=handle;
  if(id==u"buttonLan") {
    const auto parent=REX_LOAD_U32(element+40);
    if(!Valid(parent,8)||ReadText(REX_LOAD_U32(parent+4),base)!=u"aid_darkxui_frontend_coop_connection")return;
    bot_coop_handle=handle;
    updating=true;
    if(GetText(call,handle,base)!=u"BOT CO-OP")SetText(call,handle,u"BOT CO-OP",base);
    updating=false;
  }
  if((id==u"level name"||id==u"location text")&&bot_coop_handle) {
    call.r3.u64=255;sub_82779D88(call,base);
    if(call.r3.u32!=bot_coop_handle)return;
    const auto text=id==u"level name"?u"BOT CO-OP":u"Play coop offline with a bot ally.";
    updating=true;
    if(GetText(call,handle,base)!=text)SetText(call,handle,text,base);
    updating=false;
  }
}
bool ProgressionButton(PPCContext& call,uint8_t* base,uint32_t handle){
 const auto element=Element(call,handle,base);
 if(!Valid(element,44)||ReadText(REX_LOAD_U32(element+4),base)!=u"buttonLive")return false;
 const auto parent=REX_LOAD_U32(element+40);
 return Valid(parent,8)&&ReadText(REX_LOAD_U32(parent+4),base)==u"aid_darkxui_frontend_multi_connection";
}
void MultiplayerConnectionLabels(PPCContext& ctx,uint8_t* base,uint32_t rendered){
 if(!pdz_input::Supported()||busy)return;
 auto call=ctx;call.r1.u32-=1536;if(!Valid(call.r1.u32,1536))return;
 const auto element=Element(call,rendered,base);if(!Valid(element,44))return;
 const auto id=ReadText(REX_LOAD_U32(element+4),base);
 const auto connectionButton=[&](uint32_t e)->unsigned{
  if(!Valid(e,44))return 0;
  const auto parent=REX_LOAD_U32(e+40);
  if(!Valid(parent,8)||ReadText(REX_LOAD_U32(parent+4),base)!=u"aid_darkxui_frontend_multi_connection")return 0;
  const auto name=ReadText(REX_LOAD_U32(e+4),base);
  return name==u"buttonLan"?1:name==u"buttonLive"?2:0;
 };
 unsigned entry=connectionButton(element);
 const bool help=id==u"location text",title=id==u"level name";
 if(!entry&&(help||title)){
  call.r3.u64=255;sub_82779D88(call,base);
  entry=connectionButton(Element(call,call.r3.u32,base));
 }
 if(!entry)return;
 const auto text=help?(entry==1?u"Complete 30 increasingly difficult challenges":u"Rank up, Prestige, and check your combat rating"):
                      (entry==1?u"CHALLENGES":u"PROGRESSION");
 call.r3.u64=rendered;sub_8276C0F8(call,base);const auto handle=call.r3.u32;
 // Progression is a local career page; it does not require an online profile.
 busy=true;
 if(entry==2&&!help&&!title){
   call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);
 }
 if(GetText(call,handle,base)!=text)SetText(call,handle,text,base);
 busy=false;
}
// Mission scenes contain authored labels/art, independent of registry titles.
// Change their native properties only for the selected custom slot.
void SurfaceCard(PPCContext& ctx,uint8_t* base,uint32_t rendered_handle){
  static bool updating=false;
  if(updating)return;
  struct Saved {uint32_t element;std::u16string original,replacement;};
  static std::map<uint32_t,Saved> saved;
  const bool selected_surface=pdz_surface::Selected(base);
  if(!selected_surface&&saved.empty())return;
  auto call=ctx;call.r1.u32-=1536;
  if(!Valid(call.r1.u32,1536))return;
  const auto element=Element(call,rendered_handle,base);
  if(!Valid(element,44))return;
  const auto id=ReadText(REX_LOAD_U32(element+4),base);
  const bool picture=id==u"level pic1"||id==u"level pic2";
  if(!picture&&id!=u"mission number"&&id!=u"level name"&&id!=u"location text")return;
  bool card=false;
  for(auto parent=REX_LOAD_U32(element+40),depth=0u;
      parent&&depth<32&&Valid(parent,44);parent=REX_LOAD_U32(parent+40),++depth){
    if(ReadText(REX_LOAD_U32(parent+4),base).starts_with(u"aid_darkxui_frontend_level_")){card=true;break;}
  }
  if(!card)return;
  call.r3.u64=rendered_handle;sub_8276C0F8(call,base);
  const auto handle=call.r3.u32,property=picture?0x820B0CD4u:0x82001184u;
  const auto current=GetText(call,handle,base,property);
  auto old=saved.find(handle);
  if(old!=saved.end()&&(old->second.element!=element||current!=old->second.replacement)){
    saved.erase(old);old=saved.end();
  }
  updating=true;
  if(selected_surface){
    const std::u16string replacement=picture?u"pdz_surface_card":
        id==u"mission number"?u"MISSION 14":
        id==u"level name"?u"SURFACE | COLD STORAGE":u"Arctic";
    if(current!=replacement){
      if(saved.size()>128)saved.clear();
      saved[handle]={element,current,replacement};
      SetText(call,handle,replacement,base,property);
    }
  }else if(old!=saved.end()){
    SetText(call,handle,old->second.original,base,property);saved.erase(old);
  }
  updating=false;
}
// Reuse the authored Mission Mode controls so mouse, pad and keyboard agree.
std::atomic<unsigned> cheatPage{0};
std::atomic<bool> cheatBack{false};
unsigned weaponPage=0;
std::array<uint32_t,4> cheatControls{};
uint32_t cheats_entry_handle=0;
std::array<std::u16string,4> cheatOriginal{};
void CheatLabels(PPCContext& call,uint8_t* base) {
 std::array<std::u16string,4> labels;
 if(cheatPage==1)labels={u"CHALLENGES",u"DISABLE CHALLENGE",u"CHEAT LIST",u"CHEATS"};
 else if(cheatPage==2)labels={u"WEAPON ONLY",u"DISABLE CHALLENGE",u"CHEAT LIST",u"CHALLENGES"};
 else {
  const auto i=weaponPage*2; const auto count=std::size(pdz_challenge::weapons);
  labels={std::u16string(pdz_challenge::weapons[i].name),
    i+1<count?std::u16string(pdz_challenge::weapons[i+1].name):u"BACK",
    u"NEXT WEAPONS",u"WEAPON ONLY"};
 }
 for(unsigned i=0;i<4;++i)SetText(call,cheatControls[i],labels[i],base);
 call.r3.u64=cheatControls[0];call.r4.u64=255;sub_8277F5C8(call,base);
}
void CloseCheats(PPCContext& call,uint8_t* base) {
 cheatPage=0;cheatBack=false;
 for(unsigned i=0;i<4;++i)if(Element(call,cheatControls[i],base))SetText(call,cheatControls[i],cheatOriginal[i],base);
}
void BackCheats(PPCContext& call,uint8_t* base) {
 cheatBack=false;
 if(cheatPage==3&&weaponPage) --weaponPage;
 else if(cheatPage>1)cheatPage=cheatPage-1;
 else {CloseCheats(call,base);return;}
 CheatLabels(call,base);
}
bool CheatPress(PPCContext& call,uint8_t* base,uint32_t handle,std::u16string_view id) {
 if(!cheatPage) {
  if(id!=u"Leaderboards")return false;
  updateBack=false;updatePage=pdz_menu::Cheats;return true;
  const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
  call.r3.u64=handle;call.r4.u64=out;sub_8277A6F0(call,base);
  const auto parent=REX_LOAD_U32(out);
  std::array<uint32_t,4> found{Child(call,parent,u"Solo",base),Child(call,parent,u"CoOp",base),
   Child(call,parent,u"Leaderboards",base),Child(call,parent,u"header2",base)};
  for(auto& h:found) {
   if(!h||!Element(call,h,base))return false;
   call.r3.u64=h;sub_8276C0F8(call,base);h=call.r3.u32;
  }
  cheatControls=found;
  for(unsigned i=0;i<4;++i)cheatOriginal[i]=GetText(call,found[i],base);
  cheatPage=1;weaponPage=0;CheatLabels(call,base);return true;
 }
 if(id==u"BackButton"){BackCheats(call,base);return true;}
 for(unsigned i=0;i<3;++i)if(handle==cheatControls[i]) {
  if(cheatPage<3) {
   if(i==0){cheatPage=cheatPage+1;weaponPage=0;CheatLabels(call,base);}
   else if(i==1){pdz_challenge::Select(0);CloseCheats(call,base);}
   else {return true;}
  }else if(i==2){weaponPage=(weaponPage+1)%((std::size(pdz_challenge::weapons)+1)/2);CheatLabels(call,base);}
  else {
   const auto index=weaponPage*2+i;
   if(index>=std::size(pdz_challenge::weapons)){BackCheats(call,base);return true;}
   pdz_challenge::Select(pdz_challenge::weapons[index].id);
   const auto solo=cheatControls[0];CloseCheats(call,base);
   pdz_coop::SelectBotCoop(false);
   call.r3.u64=solo;call.r4.u64=0;__imp__sub_8278CC40(call,base);
  }
  return true;
 }
 return false;
}
void CheatTick(PPCContext& ctx,uint8_t* base,uint32_t rendered) {
 if(busy)return;
 auto call=ctx;call.r1.u32-=1536;if(!Valid(call.r1.u32,1536))return;
 const auto e=Element(call,rendered,base);if(!Valid(e,44))return;
 const auto id=ReadText(REX_LOAD_U32(e+4),base);
 call.r3.u64=rendered;sub_8276C0F8(call,base);const auto handle=call.r3.u32;
 busy=true;
 if(cheatPage&&cheatBack)BackCheats(call,base);
 if(id==u"Leaderboards") {
  cheats_entry_handle=handle;
  if(!cheatPage&&GetText(call,handle,base)!=u"CHEATS")SetText(call,handle,u"CHEATS",base);
  // Like the offline System Link button, only change Enabled when necessary.
  // Reassigning it every render restarts native button state/hover transitions.
  const bool enabled=true;
  call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=0;
  sub_823AE6C8(call,base);
  if(bool(call.r3.u32)!=enabled) {
   call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=enabled;call.r6.u64=0;
   sub_823AEA88(call,base);
  }
  // The repurposed entry can receive focus without its authored visual
  // leaving Normal. Drive the existing skin's standard focus range on edges.
  call.r3.u64=255;sub_82779D88(call,base);
  if(!call.r3.u32){call.r3.u64=0;sub_82779D88(call,base);}
  if(call.r3.u32)sub_8276C0F8(call,base);
  const bool focused=call.r3.u32==handle;
  const auto visual=Child(call,handle,u"DarkButton",base);
  const unsigned state=enabled?(focused?1u:0u):3u;
  static uint32_t previousVisual=0;
  static unsigned previousState=~0u;
  if(visual&&(visual!=previousVisual||state!=previousState)) {
   // This API resolves the skin from its owning control, not the visual child.
   call.r3.u64=handle;call.r4.u64=state;
   sub_8277E1D8(call,base); // Native Normal / Focus / NormalDisable ranges.
   REXLOG_INFO("Cheats visual state {} result {:08X}",state,call.r3.u32);
   if(call.r3.s32>=0){previousVisual=visual;previousState=state;}
  }
 }
 if(cheatPage&&(id==u"level name"||id==u"location text")) {
  bool cheatList=false;
  if(cheatPage==1||cheatPage==2) {
   call.r3.u64=255;sub_82779D88(call,base);
   cheatList=call.r3.u32==cheatControls[2];
   // Disabled entries retain their explanatory hover text without becoming selectable.
   const auto pointer=pdz_input::MenuPointer();
   const auto button=Element(call,cheatControls[2],base);
   if(pointer.active&&Valid(button,216)) {
    auto f=[&](unsigned offset){return std::bit_cast<float>(REX_LOAD_U32(button+offset));};
    const float x=pointer.x*1280.f-f(208),y=pointer.y*720.f-f(212);
    cheatList=cheatList||(x>=0&&y>=0&&x<=f(12)&&y<=f(16));
   }
  }
  const auto text=id==u"level name"?(cheatList?u"CHEATS":u"CHALLENGES"):
    (cheatList?u"Unlock cheats by beating target times, challenges, and achievements.":
     u"Choose one weapon and complete the story with it. New Pickups are disabled");
  if(GetText(call,handle,base)!=text)SetText(call,handle,text,base);
 }
 busy=false;
}

void Restore(PPCContext& call, uint8_t* base) {
  choosing = false; cancel = false;
  for (const auto& [handle, path] : portrait_paths)
    if (Element(call, handle, base)) SetText(call, handle, path.original, base, 0x820B0CD4);
  portrait_paths.clear();
  pdz_campaign::ReleasePortraits(call, base);
  for (const auto& [handle, text] : help_text)
    if (Element(call, handle, base)) SetText(call, handle, text.original, base);
  help_text.clear();
  for (unsigned i = 0; i < controls.size(); ++i)
    if (controls[i] && Element(call, controls[i], base)) SetText(call, controls[i], original_text[i], base);
  if (controls[0] && Element(call, controls[0], base)) {
    call.r3.u64 = controls[0]; call.r4.u64 = 255;
    sub_8277F5C8(call, base);
  }
}
bool Open(PPCContext& call, uint32_t control, uint32_t handle, uint8_t* base) {
  const uint32_t out = call.r1.u32 + 80;
  call.r3.u64 = handle; call.r4.u64 = out; REX_STORE_U32(out, 0);
  sub_8277A6F0(call, base);
  scene = REX_LOAD_U32(out);
  controls = {Child(call, scene, u"MissionMode", base), Child(call, scene, u"combat arena", base),
              Child(call, scene, u"Options", base), Child(call, scene, u"header1", base)};
  for (auto h : controls) if (!h || !Element(call, h, base)) return false;
  for (unsigned i = 0; i < controls.size(); ++i) original_text[i] = GetText(call, controls[i], base);
  const std::array<std::u16string_view, 4> names{u"MODERN", u"ORIGINAL", u"CLASSIC", u"GAME STYLE"};
  for (unsigned i = 0; i < controls.size(); ++i) SetText(call, controls[i], names[i], base);
  mission_control = control; cancel = false; choosing = true; selected = 0;
  call.r3.u64 = controls[0]; call.r4.u64 = 255;
  sub_8277F5C8(call, base);
  REXLOG_INFO("PDZ native campaign style screen opened (scene {:08X})", scene);
  return true;
}

}

namespace pdz_campaign {
bool HideForCharacterPicker(PPCContext& ctx,uint8_t* base,uint32_t handle){
 if(pdz_menu::page.load()==pdz_menu::Results)return true;
 if(pdz_menu::page.load()==pdz_menu::Cheats)return true;
 if(pdz_menu::page.load()==pdz_menu::Progression){
  auto call=ctx;if(call.r1.u32<512)return false;call.r1.u32-=512;
  if(!Valid(call.r1.u32,512))return false;
  auto e=Element(call,handle,base);
  for(unsigned depth=0;e&&depth<48&&Valid(e,44);++depth){
   const auto id=ReadText(REX_LOAD_U32(e+4),base);
   if(id.starts_with(u"button")||id==u"BackButton"||id==u"location text"||id==u"level name")return true;
   e=REX_LOAD_U32(e+40);
  }
  return false;
 }
 if(updatePage&&!busy){
  auto call=ctx;call.r1.u32-=512;if(!Valid(call.r1.u32,512))return false;
  if(extendedFromPause){
    auto e=Element(call,handle,base);
    for(unsigned depth=0;e&&depth<48&&Valid(e,44);++depth){
      const auto id=ReadText(REX_LOAD_U32(e+4),base);
      if(id.starts_with(u"button")||id==u"PauseExtendedOptions")return true;
      e=REX_LOAD_U32(e+40);
    }
    return false;
  }
  std::array<uint32_t,4> hidden{};
  for(unsigned i=0;i<4;++i)hidden[i]=Element(call,updateControls[i],base);
  auto e=Element(call,handle,base);
  for(unsigned depth=0;e&&depth<48&&Valid(e,44);++depth){
   if(updatePage==1){if(e==hidden[2])return true;}
   else {
    for(auto h:hidden)if(h&&h==e)return true;
    const auto id=ReadText(REX_LOAD_U32(e+4),base);
    if(id==u"location text"||id==u"level name")return true;
   }
   e=REX_LOAD_U32(e+40);
  }
 }

 if(!mpMenu.open||busy)return false;
 auto call=ctx;call.r1.u32-=512;if(!Valid(call.r1.u32,512))return false;
 const auto root=Element(call,mpMenu.scene,base);
 auto e=Element(call,handle,base);if(!Valid(e,240)||e==root)return false;
 // Native option handlers can show their rows again every frame. Suppress
 // their render traversal as well as their visibility while the picker owns focus.
 for(unsigned depth=0;e&&depth<48&&Valid(e,240);++depth){
  const auto id=ReadText(REX_LOAD_U32(e+4),base);
  if(id.starts_with(u"MpCharacterRow"))return false;
  if(REX_LOAD_U32(e+40)==root)return true;
  e=REX_LOAD_U32(e+40);
 }
 return false;
}
bool BackKey(uint16_t key, uint16_t flags) {
  if(pdz_menu::KeyEvent(key,flags))return true;
  if(MpKey(key,flags))return true;
  if(updatePage&&key==0x5801){if(flags&1)updateBack=true;return true;}
  if(cheatPage&&key==0x5801){if(flags&1)cheatBack=true;return true;}
  if (!choosing || key != 0x5801) return false;
  if (flags & 1) cancel = true;
  return true;
}
void Tick(PPCContext& ctx, uint8_t* base, uint32_t rendered_handle) {
  if(pdz_results::pending.load() && pdz_menu::page.load()==pdz_menu::Closed) {
    auto call=ctx;call.r1.u32-=512;
    if(Valid(call.r1.u32,512)) {
      auto e=Element(call,rendered_handle,base);
      for(unsigned depth=0;e&&depth<48&&Valid(e,44);++depth) {
        const auto id=ReadText(REX_LOAD_U32(e+4),base);
        if(id==u"aid_darkxui_ingame_large_solo_aborted" ||
           id==u"aid_darkxui_ingame_small_solo_aborted" ||
           id==u"aid_darkxui_ingame_large_solo_debriefing") {
          pdz_results::pending=false;
          pdz_results::dismiss=false;
          pdz_menu::page=pdz_menu::Results;
          break;
        }
        e=REX_LOAD_U32(e+40);
      }
    }
  }
  MpTick(ctx,base,rendered_handle);
  UpdateTick(ctx,base,rendered_handle);
  CheatTick(ctx,base,rendered_handle);
  BotCoopMenu(ctx,base,rendered_handle);
  MultiplayerConnectionLabels(ctx,base,rendered_handle);
  SurfaceCard(ctx,base,rendered_handle);
  if (!choosing || busy) return;
  auto call = ctx; call.r1.u32 -= 1536;
  if (!Valid(call.r1.u32, 1536)) return;
  if (cancel) { busy = true; Restore(call, base); busy = false; return; }
  const uint32_t element = Element(call, rendered_handle, base);
  if (!Valid(element, 44)) return;
  const auto id = ReadText(REX_LOAD_U32(element + 4), base);
  const bool portrait = id == u"level pic" || id == u"level pic1";
  if (id != u"level name" && id != u"location text" && !portrait) return;
  bool info = false;
  for (uint32_t p = REX_LOAD_U32(element + 40), depth = 0;
       p && depth < 32 && Valid(p, 44); p = REX_LOAD_U32(p + 40), ++depth) {
    if (ReadText(REX_LOAD_U32(p + 4), base).starts_with(u"aid_darkxui_frontend_info_")) {
      info = true; break;
    }
  }
  if (!info) return;
  busy = true;
  call.r3.u64 = 255; sub_82779D88(call, base);
  for (unsigned i = 0; i < 3; ++i) if (call.r3.u32 == controls[i]) selected = i;
  call.r3.u64 = rendered_handle; sub_8276C0F8(call, base);
  const uint32_t handle = call.r3.u32;
  if (portrait) {
    auto [it, fresh] = portrait_paths.try_emplace(handle);
    if (fresh) it->second.original = GetText(call, handle, base, 0x820B0CD4);
    if (!it->second.original.empty()) {
      constexpr std::u16string_view paths[]{u"pdz_style_modern", u"pdz_style_original", u"pdz_style_classic"};
      if (it->second.displayed != paths[selected]) {
        SetText(call, handle, paths[selected], base, 0x820B0CD4);
        it->second.displayed = paths[selected];
      }
    }
    busy = false;
    return;
  }
  auto [it, fresh] = help_text.try_emplace(handle);
  if (fresh) it->second.original = GetText(call, handle, base);
  constexpr std::u16string_view titles[]{u"MODERN", u"ORIGINAL", u"CLASSIC"};
  constexpr std::u16string_view descriptions[]{
      u"Full health regeneration and checkpoint retries.",
      u"Original rules for the selected difficulty.",
      u"Checkpoints heal shock only. Death restarts the mission."};
  const auto text = id == u"level name" ? titles[selected] : descriptions[selected];
  if (it->second.displayed != text) {
    SetText(call, handle, text, base); it->second.displayed = text;
  }
  busy = false;
}

}

REX_HOOK_RAW(sub_8278CCC8) {
  if (!busy && pdz_input::Supported() && Valid(ctx.r3.u32, 4) && Valid(ctx.r4.u32, 20) &&
      REX_LOAD_U32(ctx.r4.u32 + 4) == 2021) {
    const uint32_t control = ctx.r3.u32, message = ctx.r4.u32;
    const uint32_t payload = REX_LOAD_U32(message + 16);
    auto call = ctx; call.r1.u32 -= 1536;
    if (Valid(call.r1.u32, 1536) && Valid(payload, 12)) {
      // Class implementations hold alias handles; child lookups and focus use
      // the object's canonical handle. Compare the same identity throughout.
      call.r3.u64 = REX_LOAD_U32(control);
      sub_8276C0F8(call, base);
      const uint32_t handle = call.r3.u32;
      const uint32_t element = Element(call, handle, base);
      const auto id = Valid(element, 8) ? ReadText(REX_LOAD_U32(element + 4), base) : u"";
      if(!routing_bot_coop&&(id==u"Solo"||id==u"buttonLocal"||id==u"buttonLive"))pdz_coop::SelectBotCoop(false);
      bool consumed = false;
      busy = true;
      if (ProgressionButton(call,base,handle)) {
        updateBack=false;updatePage=pdz_menu::Progression;consumed=true;
      }
      else if (MpPress(call,base,handle,id)) { consumed=true; }
      else if (UpdatePress(call,base,control,message,handle,id)) { consumed=true; }
      else if (CheatPress(call,base,handle,id)) { consumed=true; }
      else if (choosing) {
        if (id == u"BackButton") { Restore(call, base); consumed = true; }
        for (unsigned i = 0; choosing && i < 3; ++i) {
          if (handle != controls[i]) continue;
          constexpr int rules[] = {1, 0, 2};
          const uint32_t saved_handle = controls[0];
          Restore(call, base);
          uint32_t live_handle = 0;
          if (Valid(mission_control, 4)) {
            call.r3.u64 = REX_LOAD_U32(mission_control);
            sub_8276C0F8(call, base);
            live_handle = call.r3.u32;
          }
          if (live_handle == saved_handle) {
            REXCVAR_SET(pdz_campaign_rules, rules[i]);
            // Continue the original Missions button's navigation, using the
            // current press message and its native user/input information.
            call.r3.u64 = mission_control; call.r4.u64 = message;
            __imp__sub_8278CCC8(call, base);
            REXLOG_INFO("PDZ native campaign style selected: {}", rules[i]);
          }
          consumed = true;
        }
      } else if (id == u"MissionMode") {
        consumed = Open(call, control, handle, base);
      }
      busy = false;
      if (consumed) { REX_STORE_U32(message + 8, 1); REX_STORE_U32(payload, 1); ctx.r3.u64 = 0; return; }
    }
  }
  __imp__sub_8278CCC8(ctx, base);
}

// The authored LAN button remains in the Co-operative scene, but uses an
// offline campaign session. Let its own navigation notification run normally.
namespace {
bool IsBotButton(PPCContext& ctx,uint8_t* base,uint32_t handle) {
  auto call=ctx;call.r1.u32-=1536;
  call.r3.u64=handle;sub_8276C0F8(call,base);
  return bot_coop_handle&&call.r3.u32==bot_coop_handle;
}
}
// Retail disc code destroys the bot manager for every DarkOps mode. Recreate
// its native object before the normal match initializer loads the bot roster.
// The normal match destructor owns it, just as it does in Deathmatch.
REX_HOOK_RAW(sub_82334C38) {
 __imp__sub_82334C38(ctx,base);
 const auto match=ctx.r3.u32;
 const auto config=REX_LOAD_U32(0x82E2ED64);
 const auto session=REX_LOAD_U32(0x82D6B2B0);
 const auto host=REX_LOAD_U32(0x82E33450);
 if(!pdz_input::Supported()||!Valid(match,1008)||!Valid(config,4375)||!Valid(session,40)||!Valid(host,37))return;
 if(REX_LOAD_U32(session+36)!=2||REX_LOAD_U32(session+32)!=0||!REX_LOAD_U8(host+36))return;
 if(REX_LOAD_U8(config+4292)!=1||!REX_LOAD_U8(config+4363)||REX_LOAD_U32(match+1004))return;
 auto c=ctx;c.r1.u32-=512;if(!Valid(c.r1.u32,512))return;
 c.r3.u64=64;c.r4.u64=10;sub_823952C8(c,base);
 if(!c.r3.u32)return;
 sub_82143668(c,base);
 REX_STORE_U32(match+1004,c.r3.u32);
 REXLOG_INFO("DarkOps: native bot manager restored for local match (requested {})",unsigned(REX_LOAD_U8(config+4363)));
}
// DarkOps has no Deathmatch map-weapon manager. The bot pickup task still
// queries it; no manager means no candidate, not a dereference of guest 0x404.
REX_HOOK_RAW(sub_8242C438) {
 if(!ctx.r3.u32&&pdz_input::Supported()){
  const auto config=REX_LOAD_U32(0x82E2ED64);
  if(Valid(config,4293)&&REX_LOAD_U8(config+4292)==1){
   if(Valid(ctx.r6.u32,4))REX_STORE_U32(ctx.r6.u32,0x7f7fffff);
   ctx.r3.u64=0;return;
  }
 }
 __imp__sub_8242C438(ctx,base);
}
// The retail DarkOps census iterates type-2 (human) participants only.
// Count real type-3 bot participants separately: they have the common player
// prefix, but NOT the human DarkOps fields at +5344 and beyond. Never change
// their type to make the human iterator accept them.
REX_EXTERN(__imp__sub_824180E0);
REX_HOOK_RAW(sub_824180E0) {
 const auto rules=ctx.r3.u32;
 __imp__sub_824180E0(ctx,base);
 const auto config=REX_LOAD_U32(0x82E2ED64);
 const auto session=REX_LOAD_U32(0x82D6B2B0);
 const auto host=REX_LOAD_U32(0x82E33450);
 if(!pdz_input::Supported()||!Valid(rules,1176)||!Valid(config,4293)||!Valid(session,40)||!Valid(host,37))return;
 if(REX_LOAD_U8(config+4292)!=1||REX_LOAD_U32(session+36)!=2||REX_LOAD_U32(session+32)!=0||!REX_LOAD_U8(host+36))return;
 auto call=ctx;if(call.r1.u32<512)return;call.r1.u32-=512;
 if(!Valid(call.r1.u32,512))return;
 auto player=REX_LOAD_U32(0x82E33464);
 for(unsigned visited=0;player&&visited<64;++visited){
  if(!Valid(player,984))break;
  const auto next=REX_LOAD_U32(player+4);
  if(REX_LOAD_U32(player+932)==3){
   const auto properties=REX_LOAD_U32(player+80);
   if(Valid(properties,124)){
    const auto property=properties+120;
    const auto table=REX_LOAD_U32(property);
    if(Valid(table,28)){
     call.r3.u64=property;
     const auto getter=REX_LOAD_U32(table+24);
     if(getter>=REX_CODE_BASE&&getter-REX_CODE_BASE<REX_CODE_SIZE){
      const auto fn=REX_LOOKUP_FUNC(base,getter);
      if(!fn)return;
      fn(call,base);
      const auto team=int8_t(call.r3.u32);
      if(team>=0&&team<4){
       // Retail DarkOps updates human allegiance in sub_82437520 but
       // omits bot actors. An empty mask makes teammates hostile to AI.
       // Refresh from the current team, including respawns/conversions.
       const auto actor=REX_LOAD_U32(player+952);
       if(Valid(actor,136))REX_STORE_U16(actor+128,uint16_t(1u<<team));
       const auto increment=[&](uint32_t offset){const auto p=rules+offset+uint32_t(team)*4;REX_STORE_U32(p,REX_LOAD_U32(p)+1);};
       increment(1160);
       call.r3.u64=player;sub_82417F80(call,base);
       if(call.r3.u32){
        increment(1112);increment(1128);
        if(!REX_LOAD_U8(player+980))increment(1144);
       }
      }
     }
    }
   }
  }
  if(next==player)break;player=next;
 }
}
// Team-score census is separate from the DarkOps round census. Keep the
// native human count/rating and add only real, currently registered bots.
static uint32_t DarkOpsScoreBotCount(PPCContext& ctx,uint8_t* base,int32_t team) {
 const auto config=REX_LOAD_U32(0x82E2ED64);
 const auto session=REX_LOAD_U32(0x82D6B2B0);
 if(!pdz_input::Supported()||team<0||team>=4||!Valid(config,4293)||!Valid(session,40))return 0;
 if(REX_LOAD_U8(config+4292)!=1||REX_LOAD_U32(session+36)!=2||REX_LOAD_U32(session+32)!=0)return 0;
 auto call=ctx;if(call.r1.u32<512)return 0;call.r1.u32-=512;
 if(!Valid(call.r1.u32,512))return 0;
 uint32_t count=0;
 auto player=REX_LOAD_U32(0x82E33464);
 for(unsigned visited=0;player&&visited<64;++visited){
  if(!Valid(player,984))break;
  const auto next=REX_LOAD_U32(player+4);
  if(REX_LOAD_U32(player+932)==3){
   const auto properties=REX_LOAD_U32(player+80);
   if(Valid(properties,124)){
    const auto table=REX_LOAD_U32(properties+120);
    if(Valid(table,28)){
     const auto getter=REX_LOAD_U32(table+24);
     if(getter>=REX_CODE_BASE&&getter-REX_CODE_BASE<REX_CODE_SIZE){
      if(const auto fn=REX_LOOKUP_FUNC(base,getter)){
       call.r3.u64=properties+120;fn(call,base);
       if(int8_t(call.r3.u32)==team)++count;
      }
     }
    }
   }
  }
  if(next==player)break;
  player=next;
 }
 return count;
}
REX_EXTERN(__imp__sub_82436FC0);
REX_HOOK_RAW(sub_82436FC0) {
 const auto team=ctx.r3.s32;
 const auto bots=DarkOpsScoreBotCount(ctx,base,team);
 __imp__sub_82436FC0(ctx,base);
 ctx.r3.u64=ctx.r3.u32+bots;
}
REX_EXTERN(__imp__sub_824292C8);
REX_HOOK_RAW(sub_824292C8) {
 const auto team=ctx.r3.s32;
 const auto bots=DarkOpsScoreBotCount(ctx,base,team);
 __imp__sub_824292C8(ctx,base);
 // sub_82429388 assigns native bots a rating of one. Include that in the
 // numerator too, so the scoreboard's average uses the same roster.
 ctx.r3.u64=ctx.r3.u32+bots;
}
// The bot manager was authored for Deathmatch and drains its respawn queue
// without consulting DarkOps round lives. Hold death entries during an
// elimination round, but retain them so the next countdown revives the bots.
REX_EXTERN(__imp__sub_82143D58);
REX_HOOK_RAW(sub_82143D58) {
 const auto manager=ctx.r3.u32;
 const auto config=REX_LOAD_U32(0x82E2ED64);
 const auto session=REX_LOAD_U32(0x82D6B2B0);
 const auto rules=REX_LOAD_U32(0x82E2E894);
 if(pdz_input::Supported()&&Valid(manager,64)&&Valid(config,4322)&&Valid(session,40)&&Valid(rules,1112)
    &&REX_LOAD_U32(session+36)==2&&REX_LOAD_U32(session+32)==0&&REX_LOAD_U8(config+4292)==1){
  const auto mode=REX_LOAD_U8(config+4321);
  const auto state=REX_LOAD_U32(rules+1108);
  if((mode==0||mode==1||mode==3)&&state>=2){
   const auto count=REX_LOAD_U32(manager+48);
   const auto queue=REX_LOAD_U32(manager+40);
   if(count<=64&&Valid(queue,count*4)){
    for(uint32_t i=0;i<count;++i){
     const auto entry=REX_LOAD_U32(queue+i*4);
     if(!Valid(entry,12)||REX_LOAD_U8(entry+8))continue; // initial creation
     const auto player=REX_LOAD_U32(entry);
     if(!Valid(player,984)||REX_LOAD_U32(player+932)!=3)continue;
     if(mode==1){
      // Onslaught: only the attacking team (rules +1180) has reinforcements.
      const auto properties=REX_LOAD_U32(player+80);
      if(Valid(rules,1184)&&Valid(properties,124)&&ctx.r1.u32>=512){
       const auto table=REX_LOAD_U32(properties+120);
       if(Valid(table,28)){
        const auto getter=REX_LOAD_U32(table+24);
        if(getter>=REX_CODE_BASE&&getter-REX_CODE_BASE<REX_CODE_SIZE){
         auto call=ctx;call.r1.u32-=512;call.r3.u64=properties+120;
         if(Valid(call.r1.u32,512)){
          if(const auto fn=REX_LOOKUP_FUNC(base,getter)){
           fn(call,base);
           if(int8_t(call.r3.u32)==int32_t(REX_LOAD_U32(rules+1180)))continue;
          }
         }
        }
       }
      }
     }
     // Native tick subtracts this frame's delta, then revives only below zero.
     const float delay=float(ctx.f1.f64)+1.0f;
     REX_STORE_U32(entry+4,std::bit_cast<uint32_t>(delay));
    }
   }
  }
 }
 __imp__sub_82143D58(ctx,base);
}
REX_EXTERN(__imp__sub_8241C6E0);
REX_HOOK_RAW(sub_8241C6E0) {
 const auto player=ctx.r4.u32;
 if(!pdz_input::Supported()||!Valid(player,984)||REX_LOAD_U32(player+932)!=3){
  __imp__sub_8241C6E0(ctx,base);return;
 }
 // Infection's cached pad property at +5448 belongs to the human subtype.
 // Bots use the same native safe-pad search, without that human-only cache.
 auto call=ctx;
 if(call.r1.u32<512){ctx.r3.u64=0;return;}
 call.r1.u32-=512;
 if(!Valid(call.r1.u32,512)){ctx.r3.u64=0;return;}
 sub_8241C4E8(call,base);
 auto pad=call.r3.u32;
 if(!pad){
  const auto map=REX_LOAD_U32(0x82D6B340);
  const auto pads=Valid(map,1104)?REX_LOAD_U32(map+1100):0;
  const auto count=Valid(pads,4)?REX_LOAD_U32(pads):0;
  // Bounded equivalent of retail's random retry fallback.
  for(uint32_t i=0;i<count&&i<65536;++i){
   call.r3.u64=i;sub_821659D0(call,base);
   if(call.r3.u32)continue;
   call.r3.u64=call.r1.u32+256;sub_82165960(call,base);
   pad=REX_LOAD_U32(call.r1.u32+256);if(pad)break;
  }
 }
 if(!Valid(pad,28)){ctx.r3.u64=0;return;}
 call.r3.u64=REX_LOAD_U32(player+952);call.r4.u64=pad+16;
 call.r6.s64=-1;call.r7.u64=0;call.r8.u64=1;call.r9.u64=1;call.r10.u64=1;
 call.f1.f64=std::bit_cast<float>(REX_LOAD_U32(0x8208E390));
 REX_STORE_U32(call.r1.u32+84,0xffffffff);
 REX_STORE_U32(call.r1.u32+92,0);REX_STORE_U32(call.r1.u32+100,0);
 REX_STORE_U8(call.r1.u32+111,1);
 sub_82165378(call,base);
 ctx.r3.u64=call.r3.u32?call.r3.u32:pad;
}
REX_EXTERN(__imp__sub_823F4F00);
REX_HOOK_RAW(sub_823F4F00) {
  if(pdz_input::Supported()&&Valid(ctx.r3.u32,12)){
    auto call=ctx;if(call.r1.u32>=1536){call.r1.u32-=1536;
      if(Valid(call.r1.u32,1536)&&ProgressionButton(call,base,REX_LOAD_U32(ctx.r3.u32+4))){
        call.r3.u64=REX_LOAD_U32(ctx.r3.u32+4);call.r4.u64=0x820B1418;
        call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);ctx.r3.u64=0;return;
      }
    }
  }
  // The repurposed Leaderboards entry must not be disabled by retail's
  // network/profile gate between CheatTick and the native pointer hit test.
  if(cheats_entry_handle&&Valid(ctx.r3.u32,12)) {
    auto call=ctx;call.r1.u32-=1536;
    if(Valid(call.r1.u32,1536)) {
      call.r3.u64=REX_LOAD_U32(ctx.r3.u32+4);sub_8276C0F8(call,base);
      const auto handle=call.r3.u32;
      const auto element=Element(call,handle,base);
      if(handle==cheats_entry_handle&&Valid(element,8)&&ReadText(REX_LOAD_U32(element+4),base)==u"Leaderboards") {
        const bool enabled=true;
        call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=0;sub_823AE6C8(call,base);
        if(bool(call.r3.u32)!=enabled) {
          call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=enabled;call.r6.u64=0;sub_823AEA88(call,base);
        }
        ctx.r3.u64=0;return;
      }
    }
  }
  if(Valid(ctx.r3.u32,12)&&IsBotButton(ctx,base,REX_LOAD_U32(ctx.r3.u32+4))) {
    auto call=ctx;call.r1.u32-=1536;
    const auto handle=REX_LOAD_U32(ctx.r3.u32+4);
    call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=0;
    sub_823AE6C8(call,base);
    if(!call.r3.u32) {
      call.r3.u64=handle;call.r4.u64=0x820B1418; // Enabled
      call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);
    }
    ctx.r3.u64=0;return;
  }
  __imp__sub_823F4F00(ctx,base);
}
REX_EXTERN(__imp__sub_823F5018);
REX_HOOK_RAW(sub_823F5018) {
  if(pdz_input::Supported()&&Valid(ctx.r3.u32,12)){
    auto call=ctx;if(call.r1.u32>=1536){call.r1.u32-=1536;
      if(Valid(call.r1.u32,1536)&&ProgressionButton(call,base,REX_LOAD_U32(ctx.r3.u32+4))){
        updateBack=false;updatePage=pdz_menu::Progression;ctx.r3.u64=0;return;
      }
    }
  }
  if(Valid(ctx.r3.u32,12)&&IsBotButton(ctx,base,REX_LOAD_U32(ctx.r3.u32+4))) {
    auto call=ctx;call.r1.u32-=1536;
    pdz_coop::SelectBotCoop(true);REXCVAR_SET(pdz_coop_ally,true);
    call.r3.u64=78;call.r4.u64=pdz_coop::NativePartnerSelected()?1:0;sub_823B0388(call,base);
    call.r3.u64=79;call.r4.u64=0;sub_823B0388(call,base);
    // Action 78 updates the pending configuration, while this scene checks
    // the live session immediately for a second local controller.
    const auto session=REX_LOAD_U32(0x82D6B2B0);
    if(Valid(session,40))REX_STORE_U32(session+36,pdz_coop::NativePartnerSelected()?1:0);
    REXLOG_INFO("PDZ Bot Co-op: local offline connection selected");
    ctx.r3.u64=0;return;
  }
  __imp__sub_823F5018(ctx,base);
}

REX_HOOK_RAW(sub_8278CC40) {
  if(bot_coop_handle&&local_coop_handle&&IsBotButton(ctx,base,ctx.r3.u32)) {
    ctx.r3.u64=local_coop_handle;
    REXLOG_INFO("PDZ Bot Co-op: local scene notification");
  }
  __imp__sub_8278CC40(ctx,base);
}

// This authored Co-operative scene behavior starts timer 1101 and opens the
// second-controller identification dialog. A bot has no controller to identify.
REX_EXTERN(__imp__sub_823C1F10);
REX_HOOK_RAW(sub_823C1F10) {
  if(pdz_coop::NativePartnerSelected()) {
    // Let the native controller scan run. The SDK supplies controller 1;
    // do not fabricate a profile pointer or fall back to Player 1.
    __imp__sub_823C1F10(ctx,base);
    return;
  }
  if(pdz_coop::BotCoopSelected()&&Valid(ctx.r3.u32,24)) {
    REX_STORE_U8(ctx.r3.u32+20,0);
    REXLOG_INFO("PDZ Bot Co-op: bypassed second-controller identification");
    ctx.r3.u64=0;return;
  }
  __imp__sub_823C1F10(ctx,base);
}

// Bot loadout temporarily mirrors the primary profile. Never return a missing
// Player 2 profile to the native loadout editor (it dereferences +9444).
REX_EXTERN(__imp__sub_824FB718);
REX_HOOK_RAW(sub_824FB718) {
  if(pdz_coop::BotCoopSelected()&&!pdz_coop::NativePartnerSelected()) {
    constexpr uint32_t users=0x82D2CA58;
    const auto primary=REX_LOAD_U8(users+20);
    const auto secondary=REX_LOAD_U8(users+21);
    if(primary<4&&(ctx.r3.u32==secondary||ctx.r3.u32==255))ctx.r3.u64=primary;
  }
  const auto requested_profile=ctx.r3.u32;
  __imp__sub_824FB718(ctx,base);
  const auto primary=REX_LOAD_U8(0x82D2CA58+20);
  if(requested_profile==(primary==255?0:primary)) {
    std::array<std::array<unsigned char,8>,14> records{};
    const auto profile=ctx.r3.u32;
    if(Valid(profile,8036+14*12))
      for(unsigned mission=0;mission<14;++mission)
        for(unsigned difficulty=0;difficulty<8;++difficulty)
          records[mission][difficulty]=REX_LOAD_U8(profile+8036+mission*12+difficulty);
    pdz_custom::campaign_unlocked=pdz_custom::DarkCampaignComplete(records);
  }
  pdz_challenge::ForceLoadout(base,ctx.r3.u32);
}
REX_EXTERN(__imp__sub_8239A530);
REX_HOOK_RAW(sub_8239A530) {
  __imp__sub_8239A530(ctx,base);
  if(pdz_coop::BotCoopSelected()&&!pdz_coop::NativePartnerSelected()&&ctx.r3.u32==255) {
    const auto primary=REX_LOAD_U8(0x82D2CA58+20);
    if(primary<4)ctx.r3.u64=primary;
  }
}

// The local co-op launch behavior requires users[1], even after its controller
// prompt is bypassed. Bot co-op uses one human profile and one rendered player,
// so launch through the native single-player behavior instead.
REX_EXTERN(__imp__sub_823C1880);
// The virtual partner has an ephemeral profile. Choose the native "continue
// without saving" transition before its controller-owned save prompt opens.
// This is the same state/flag pair used by sub_82501B58, scoped to guest 1.
REX_EXTERN(__imp__sub_82501738);
REX_HOOK_RAW(sub_82501738) {
  if(pdz_coop::NativePartnerSelected()&&ctx.r4.u32==2&&
     Valid(ctx.r3.u32,12757)&&REX_LOAD_U32(ctx.r3.u32+9960)==1) {
    REX_STORE_U8(ctx.r3.u32+9953,0);
    ctx.r4.u64=29;
    REXLOG_INFO("Native Bot Co-op: guest continuing without save storage");
  }
  __imp__sub_82501738(ctx,base);
}
REX_HOOK_RAW(sub_823C1880) {
  if(pdz_coop::NativePartnerSelected()) {
    constexpr uint32_t users=0x82D2CA58;
    const auto primary=REX_LOAD_U8(users+20),secondary=REX_LOAD_U8(users+21);
    auto lookup=ctx;lookup.r1.u32-=1536;lookup.r3.u64=primary;
    __imp__sub_824FB718(lookup,base);const auto first=lookup.r3.u32;
    lookup.r3.u64=secondary;__imp__sub_824FB718(lookup,base);const auto second=lookup.r3.u32;
    if(primary!=0||secondary!=1||first==second||!Valid(first,11112)||!Valid(second,11112)||
       REX_LOAD_U32(first+11100)!=30||REX_LOAD_U32(second+11100)!=30) {
      REXLOG_WARN("Native Bot Co-op: waiting for distinct profiles (users {} {}, profiles {:08X} {:08X})",primary,secondary,first,second);
      ctx.r3.u64=0;return;
    }
    REXLOG_INFO("Native Bot Co-op: launching native partner with separate profile {:08X}",second);
    __imp__sub_823C1880(ctx,base);return;
  }
  if(pdz_coop::BotCoopSelected()) {
    REXLOG_INFO("PDZ Bot Co-op: starting mission with primary player profile");
    sub_823FBFF0(ctx,base);
    return;
  }
  __imp__sub_823C1880(ctx,base);
}

// Native event 2021 can reach a cached scene while the host options overlay
// owns input. The original handler dereferences a failed player-menu lookup
// at 823FC278 (+1152), as confirmed by the 2026-10-11 crash dump.
// Preserve its successful path, but ignore overlay input and missing objects.
REX_HOOK_RAW(sub_823FC208) {
  if(pdz_menu::Overlay()){ctx.r3.u64=0;return;}
  const uint32_t owner=ctx.r3.u32,handled=ctx.r5.u32;
  auto call=ctx;call.r1.u32-=256;
  ctx.r3.u64=0;
  if(!Valid(owner,8)||!Valid(call.r1.u32,256)||!Valid(handled,4))return;
  const uint32_t scratch=call.r1.u32+80;
  REX_STORE_U32(scratch,0);
  call.r3.u64=REX_LOAD_U32(owner+4);call.r4.u64=scratch;
  sub_8277A6F0(call,base);
  call.r3.u64=REX_LOAD_U32(scratch);
  sub_8277DEA8(call,base);
  const uint32_t sceneObject=call.r3.u32;
  sub_8276B9B8(call,base);
  if(!call.r3.u32)return;
  REX_STORE_U32(scratch+4,REX_LOAD_U32(owner+4));
  call.r3.u64=scratch+4;call.r4.u64=REX_LOAD_U32(scratch+4);
  sub_823AF4B0(call,base);
  if(!Valid(call.r3.u32,4)||REX_LOAD_U32(call.r3.u32))return;
  call.r3.u64=sceneObject;
  sub_82779E50(call,base);
  sub_823AC0F8(call,base);
  if(!Valid(call.r3.u32,1156)){
    REXLOG_WARN("Ignored native menu event with no matching player-menu object");
    return;
  }
  const uint32_t index=REX_LOAD_U32(call.r3.u32+1152)&255;
  call.r3.u64=index<4?REX_LOAD_U32(0x82E2E7C4+index*4):0;
  call.r4.u64=0;
  sub_823A0C30(call,base);
  REX_STORE_U32(handled,1);
}

// Native state 24 finalizes these records before entering the mission-over
// scene. Copy the result after that calculation; never alter the native stats.
REX_HOOK_RAW(sub_8222A9D0) {
  const auto profile=ctx.r3.u32;
  const auto session=REX_LOAD_U32(0x82D6B2B0);
  const bool completed=(REX_LOAD_U8(0x82E41930)&3)==1;
  __imp__sub_8222A9D0(ctx,base);
  if(!pdz_input::Supported() || !completed || !Valid(session,40) ||
     REX_LOAD_U32(session+36)!=0 || !Valid(profile,7363))return;
  pdz_results::Summary s{};
  s.completed=true;
  s.mission=REX_LOAD_U8(session+25);
  s.difficulty=REX_LOAD_U32(session+4);
  s.campaign_style=unsigned(REXCVAR_GET(pdz_campaign_rules));
  s.time_cs=REX_LOAD_U32(profile+5464);
  s.kills=REX_LOAD_U32(profile+5416)+REX_LOAD_U32(profile+5420);
  s.shots=REX_LOAD_U32(profile+5468);
  s.head_hits=REX_LOAD_U32(profile+5472);
  s.body_hits=REX_LOAD_U32(profile+5476);
  s.limb_hits=REX_LOAD_U32(profile+5480);
  auto call=ctx;call.r1.u32-=1536;
  if(Valid(call.r1.u32,1536)) {
    sub_8226F830(call,base);
    if(Valid(call.r3.u32,236))
      s.target_seconds=REX_LOAD_U32(call.r3.u32+224+4*std::min(s.difficulty,2u));
  }
  // The native debriefing's stored overall-best record uses this same layout.
  if(s.mission<44) {
    const auto best=REX_LOAD_U32(profile+24+s.mission*120+48);
    s.best_cs=best?std::min(best,s.time_cs):s.time_cs;
  }
  s.newly_unlocked=pdz_cheats::progress.Complete(s);
  {std::lock_guard lock(pdz_results::mutex);pdz_results::summary=s;}
  pdz_results::pending=true;
  REXLOG_INFO("PD summary captured: mission {} time {}cs target {}s",s.mission,s.time_cs,s.target_seconds);
}

REX_HOOK_RAW(sub_82229E68) {
  pdz_results::pending=false;
  if(pdz_menu::page.load()==pdz_menu::Results)pdz_menu::page=pdz_menu::Closed;
  __imp__sub_82229E68(ctx,base);
}



