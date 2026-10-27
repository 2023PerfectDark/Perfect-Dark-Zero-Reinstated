#include "generated/default/perfectdarkzerorecomp_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <cmath>
#include <chrono>
#include "notification_timeout.h"
#include <atomic>
#include <mutex>
#include <fstream>
#include <map>
#include <bit>
#include <string>
#include <algorithm>
#include <rex/cvar.h>
#include "campaign_menu.h"
#include "game_cvars.h"
#include "mousehook.h"
#include "pd_menu_state.h"

REXCVAR_DEFINE_BOOL(pdz_pointer_trace, false, "Debug", "Bounded native menu hit-test diagnostics");

namespace {
bool Range(uint32_t p, uint32_t n) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !p || uint64_t(p) + n > 0x100000000ull) return false;
  for (uint64_t a = p; a < uint64_t(p) + n;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(a));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(a), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    a = (a / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
}

namespace {
bool Visible(uint32_t element, uint8_t* base) {
  for (int depth = 0; element && depth < 64; ++depth) {
    if (!Range(element, 240) || !(REX_LOAD_U32(element + 236) & 1)) return false;
    element = REX_LOAD_U32(element + 40);
    if (!element) return true;
  }
  return false;
}
std::u16string Id(uint32_t element, uint8_t* base) {
  std::u16string id;
  if (!Range(element, 8)) return id;
  const auto p = REX_LOAD_U32(element + 4);
  for (unsigned i = 0; p && i < 96 && Range(p + i * 2, 2); ++i) {
    const auto c = REX_LOAD_U16(p + i * 2);
    if (!c) break;
    id += char16_t(c);
  }
  return id;
}
// UI-thread state only. Never clear the game's unread-message state.
std::map<uint32_t, pdz_ui::NotificationTimeout> notificationTimeouts;
double NotificationNow() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint32_t PauseOwner(uint32_t element, uint8_t* base) {
  for (unsigned i = 0; element && i < 64 && Range(element, 44); ++i) {
    if (Id(element, base) == u"aid_darkxui_ingame_large_solo_start") return element;
    element = REX_LOAD_U32(element + 40);
  }
  return 0;
}
}

// Observe actual render messages, never inventory data lookups: those are also
// called while the inventory UI is closed and must not capture gameplay input.
REX_EXTERN(__imp__sub_8239DDD8);
REX_HOOK_RAW(sub_8239DDD8) {
  if (pdz_input::Supported() && Range(ctx.r5.u32, 8) && Range(ctx.r4.u32, 20) &&
      REX_LOAD_U32(ctx.r4.u32 + 4) == 0) {
    const uint32_t table = REX_LOAD_U32(ctx.r5.u32);
    const bool category = table == 0x820B47AC ||
        (table >= 0x820B6450 && table <= 0x820B652C && (table - 0x820B6450) % 44 == 0);
    if (category) {
      auto call = ctx;
      call.r1.u32 -= 256;
      if (!Range(call.r1.u32, 256)) { __imp__sub_8239DDD8(ctx, base); return; }
      call.r3.u64 = REX_LOAD_U32(ctx.r5.u32 + 4);
      call.r4.u64 = REX_LOAD_U32(0x82D356C4); // registered XuiElement class
      sub_82779A50(call, base);
      if (Visible(call.r3.u32, base)) pdz_input::InventoryMenuRendered();
    }
  }
  __imp__sub_8239DDD8(ctx, base);
}

// Native XuiElement visibility property (18). Re-arm on a fresh native
// notification, but repeated "visible" writes must not extend its lifetime.
REX_EXTERN(__imp__sub_82798558);
REX_HOOK_RAW(sub_82798558) {
  const auto element = ctx.r3.u32, message = ctx.r4.u32;
  if (pdz_input::Supported() && Range(message,20) &&
      REX_LOAD_U32(message+4)==18 && Range(element,240) &&
      Id(element,base)==u"imageObjectiveMessages") {
    const auto value = REX_LOAD_U32(message+16);
    if (Range(value,4) && (!REX_LOAD_U32(value) || !(REX_LOAD_U32(element+236)&1)))
      notificationTimeouts.erase(element);
  }
  __imp__sub_82798558(ctx,base);
}

REX_EXTERN(__imp__sub_827729F8);
REX_HOOK_RAW(sub_827729F8) {
  const uint32_t render_handle = ctx.r3.u32;
  const bool render = pdz_input::Supported() && Range(ctx.r4.u32, 20) &&
      REX_LOAD_U32(ctx.r4.u32 + 4) == 0;
  if(render){
    auto call=ctx;call.r1.u32-=256;
    if(Range(call.r1.u32,256)){
      call.r3.u64=render_handle;call.r4.u64=REX_LOAD_U32(0x82D356C4);
      sub_82779A50(call,base);
      // Hide only the HUD scene, leaving d-PAL and host options accessible.
      const auto element=call.r3.u32;
      const auto id=Id(element,base);
      if(REXCVAR_GET(pdz_hide_hud)&&id.starts_with(u"aid_darkxui_ingame_hud")){ctx.r3.u64=0;return;}
      if(id==u"imageObjectiveMessages"){
        if(notificationTimeouts.size()>32)notificationTimeouts.clear();
        if(notificationTimeouts[element].Hidden(NotificationNow(),Visible(element,base))){
          ctx.r3.u64=0;return;
        }
      }
    }
  }
  if (render) pdz_campaign::Tick(ctx, base, render_handle);
  if(render&&pdz_campaign::HideForCharacterPicker(ctx,base,render_handle)){ctx.r3.u64=0;return;}
  __imp__sub_827729F8(ctx, base);
  if (!render || pdz_menu::Overlay()) return;
  const auto pointer = pdz_input::MenuPointer();
  static uint32_t drag_element = 0;
  static uint64_t drag_press = 0;
  static float drag_left = 0, drag_width = 0, drag_offset = 0;
  if (!pointer.active || !pointer.held || pointer.press != drag_press)
    drag_element = 0;
  if (!pointer.active) return;
  struct Trace {
    uint32_t handle, element=0, stage=0; pdz_input::Pointer pointer; uint8_t* base;
    ~Trace(){
      // Bounded click-only diagnostics for native settings dispatch failures.
      static unsigned clicks = 0;
      if (pointer.click && element && Range(element, 240) && clicks < 4096) {
        ++clicks;
        const auto id = Id(element, base);
        std::ofstream log("menu-arrow-clicks.csv", std::ios::app);
        log << pointer.click << ',' << std::string(id.begin(), id.end()) << ','
            << stage << ',' << pointer.x * 1280.f << ',' << pointer.y * 720.f;
        for (auto offset : {12u,16u,208u,212u})
          log << ',' << std::bit_cast<float>(REX_LOAD_U32(element + offset));
        log << '\n';
      }
      static unsigned count=0;
      static std::map<uint32_t,uint64_t> motions;
      if(!REXCVAR_GET(pdz_pointer_trace)||stage<5||count>=6000)return;
      auto it=motions.find(handle);
      if(it!=motions.end()&&it->second==pointer.motion)return;
      motions[handle]=pointer.motion;
      ++count;
      std::ofstream out("menu-pointer-trace.csv",std::ios::app);
      out<<handle<<','<<element<<','<<stage<<','<<pointer.x<<','<<pointer.y<<','<<pointer.motion;
      for(auto offset:{12u,16u,208u,212u})out<<','<<std::bit_cast<float>(REX_LOAD_U32(element+offset));
      out<<'\n';
    }
  } trace{render_handle,0,0,pointer,base};
  // A stationary mouse must not reclaim focus during native Back navigation.
  static uint64_t last_motion = 0;
  if (last_motion == pointer.motion && !pointer.click && !pointer.held) return;
  // All calls stay on the guest UI thread, with a separate guest stack frame.
  auto call = ctx;
  call.r1.u32 -= 256;
  const uint32_t scratch = call.r1.u32 + 80;
  if (!Range(call.r1.u32, 256)) return;
  call.r3.u64 = render_handle;
  sub_8276C0F8(call, base);
  const uint32_t handle = call.r3.u32;
  call.r3.u64 = handle; call.r4.u64 = REX_LOAD_U32(0x82D356C4);
  sub_82779A50(call, base);
  const uint32_t element = call.r3.u32;
  trace.element=element;trace.stage=1;
  if (!Visible(element, base)) return;
  trace.stage=2;
  // Cached scenes share the background root during transitions. Compare the
  // nearest XuiScene, not that shared root, or the old title can steal focus.
  uint32_t focus_user = 255;
  call.r3.u64 = focus_user;
  sub_82779D88(call, base);
  if (!call.r3.u32) {
    focus_user = 0; // In-game d-PAL uses the local player focus slot.
    call.r3.u64 = focus_user;
    sub_82779D88(call, base);
  }
  if (!call.r3.u32) return;
  trace.stage=3;
  const uint32_t focused_handle = call.r3.u32;
  call.r4.u64 = REX_LOAD_U32(0x82D356C4);
  sub_82779A50(call, base);
  uint32_t focused_root = call.r3.u32, target_root = element;
  // d-PAL tab labels and footer legends are not ordinary focusable buttons.
  // Keep focus on the active page when invoking its A/B/Start action.
  const auto pause = PauseOwner(element, base);
  const auto id = Id(element, base);
  // The red knob is a visual child of the native settings slider. Capture its
  // owning slider for the whole press, even when the cursor leaves the track.
  if (id == u"slider dot") {
    uint32_t slider_element = element;
    for (unsigned depth = 0; depth < 8 && Range(slider_element, 240); ++depth) {
      const auto name = Id(slider_element, base);
      if (name == u"sliderSoundVolume" || name == u"sliderMusicVolume" ||
          name == u"sliderBrightness") break;
      slider_element = REX_LOAD_U32(slider_element + 40);
    }
    const auto name = Id(slider_element, base);
    if (name != u"sliderSoundVolume" && name != u"sliderMusicVolume" &&
        name != u"sliderBrightness") return;
    if (!Visible(slider_element, base)) return;
    call.r3.u64 = REX_LOAD_U32(slider_element); sub_8276C0F8(call, base);
    const uint32_t slider_handle = call.r3.u32;
    call.r4.u64 = REX_LOAD_U32(0x82D366B8); // Native slider class.
    sub_82779A50(call, base);
    const uint32_t slider = call.r3.u32;
    if (!Range(slider, 32)) return;
    const int minimum = int32_t(REX_LOAD_U32(slider + 16));
    const int maximum = int32_t(REX_LOAD_U32(slider + 20));
    const int value = int32_t(REX_LOAD_U32(slider + 24));
    if (name == u"sliderBrightness" ? (minimum != -10 || maximum != 10) :
        (minimum != 0 || maximum != 100)) return;
    const float span = float(maximum - minimum);
    auto f = [base](uint32_t p) { return std::bit_cast<float>(REX_LOAD_U32(p)); };
    const float dot_x = f(element + 208) + f(element + 12) * .5f;
    const float dot_y = f(element + 212) + f(element + 16) * .5f;
    const uint32_t bar = REX_LOAD_U32(element + 40);
    if (!Range(bar, 44)) return;
    const uint32_t body = REX_LOAD_U32(bar + 40);
    if (!Range(body, 240) || Id(body, base) != u"SliderBody") return;
    const float width = f(body + 12);
    if (!std::isfinite(dot_x) || !std::isfinite(dot_y) ||
        !std::isfinite(width) || width < 10 || width > 1000) return;
    const float left = dot_x - width * (float(value - minimum) / span);
    if (!drag_element && pointer.held && pointer.press_pending &&
        std::abs(pointer.press_y * 720.f - dot_y) <= 14.f &&
        pointer.press_x * 1280.f >= left - 12.f &&
        pointer.press_x * 1280.f <= left + width + 12.f) {
      drag_element = element; drag_press = pointer.press;
      drag_left = left; drag_width = width;
      const float offset = pointer.press_x * 1280.f - dot_x;
      drag_offset = std::abs(offset) <= 12.f ? offset : 0.f;
      pdz_input::PointerAccept(pointer.press, 0);
      call.r3.u64 = slider_handle; call.r4.u64 = focus_user;
      sub_8277F5C8(call, base);
    }
    if (drag_element == element) {
      const float fraction = (pointer.x * 1280.f - drag_offset - drag_left) / drag_width;
      if (!std::isfinite(fraction)) return;
      const int target = minimum + int(std::lround(std::clamp(fraction, 0.f, 1.f) * span));
      if (target != value) {
        // Sends the native changing/changed notifications, updating the setting,
        // label, knob animation and profile persistence together.
        call.r3.u64 = slider_handle; call.r4.u64 = target;
        sub_8277BBB8(call, base);
      }
      last_motion = pointer.motion;
    }
    return;
  }
  if (drag_element) return;
  if (pause && (id == u"A button" || id == u"B button" ||
                id == u"XuiNavButton1" || id == u"XuiBackButton1")) return;
  const bool tab = id == u"TabLabel";
  const uint16_t legend = id == u"A_Button" ? 0x5800 :
      id == u"B_Button" ? 0x5801 : id == u"Start_button" ? 0x5814 : 0;
  if (pause && pause == PauseOwner(focused_root, base) && (tab || legend)) {
    if (!pointer.click) return;
    auto f = [base](uint32_t p) { return std::bit_cast<float>(REX_LOAD_U32(p)); };
    const float width = f(element + 12), height = f(element + 16);
    const float x = pointer.x * 1280.f - f(element + 208);
    const float y = pointer.y * 720.f - f(element + 212);
    // Footer assets include wide blank left padding; their visible legends
    // occupy the final 200 units, with adjacent prompts 200 units apart.
    if (!std::isfinite(x) || !std::isfinite(y) || x < (legend ? std::max(0.f, width - 200.f) : 0.f) ||
        x > width || y < 0 || y > std::min(height, 40.f)) return;
    if (legend) { pdz_input::PointerAccept(pointer.click, legend); return; }
    const auto parent_id = Id(REX_LOAD_U32(element + 40), base);
    const int index = parent_id == u"Button1" ? 0 : parent_id == u"Button2" ? 1 : parent_id == u"Button3" ? 2 : -1;
    if (index < 0) return;
    call.r3.u64 = REX_LOAD_U32(pause); sub_8276C0F8(call, base);
    call.r4.u64 = REX_LOAD_U32(0x82D356C0); // XuiTabScene
    sub_82779A50(call, base);
    if (!Range(call.r3.u32, 24) || REX_LOAD_U32(call.r3.u32 + 8) <= uint32_t(index)) return;
    pdz_input::PointerAccept(pointer.click, 0);
    call.r4.u64 = index; call.r5.u64 = 1;
    sub_82792F10(call, base); // Native tab selection and page focus transfer.
    return;
  }
  auto root = [&call, base](uint32_t p) {
    for (unsigned depth = 0; p && depth < 64; ++depth) {
      if (!Range(p, 44)) return uint32_t(0);
      call.r3.u64 = REX_LOAD_U32(p);
      sub_8276C0F8(call, base);
      call.r4.u64 = REX_LOAD_U32(0x82D356D4); // XuiScene
      sub_82779A50(call, base);
      if (call.r3.u32) return p;
      const auto parent = REX_LOAD_U32(p + 40);
      if (!parent) return p;
      p = parent;
    }
    return uint32_t(0);
  };
  // Multiplayer value rows contain their own XuiScene. They still belong
  // to one settings page, so compare the owning multiplayer page instead.
  auto multiplayer_page = [base](uint32_t p) {
    for (unsigned depth = 0; p && depth < 64 && Range(p, 44); ++depth) {
      if (Id(p, base).starts_with(u"aid_darkxui_frontend_multi_")) return p;
      p = REX_LOAD_U32(p + 40);
    }
    return uint32_t(0);
  };
  const auto multiplayer = multiplayer_page(target_root);
  if (!focused_root || (multiplayer ? multiplayer_page(focused_root) != multiplayer :
      root(focused_root) != root(target_root))) return;
  const bool postgame = multiplayer &&
      Id(multiplayer, base) == u"aid_darkxui_frontend_multi_statspostgame";
  const bool owner_lobby = multiplayer &&
      Id(multiplayer, base) == u"aid_darkxui_frontend_multi_gamelobby_owner";
  if ((postgame || owner_lobby) && id == u"TabLabel_large") {
    if (postgame && !pointer.click) return;
    auto f = [base](uint32_t p) { return std::bit_cast<float>(REX_LOAD_U32(p)); };
    const float x = pointer.x * 1280.f - f(element + 208);
    const float y = pointer.y * 720.f - f(element + 212);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 ||
        x > f(element + 12) || y > f(element + 16)) return;
    const auto parent = REX_LOAD_U32(element + 40);
    if (!Range(parent, 44)) return;
    const auto name = Id(parent, base);
    if (name.size() != 7 || !name.starts_with(u"Button") ||
        name[6] < u'1' || name[6] > u'6') return;
    const uint32_t index = name[6] - u'1';
    const auto owner = REX_LOAD_U32(parent + 40);
    if (!Range(owner, 44)) return;
    if (postgame ? Id(owner, base) != u"resultTabscene" :
        (Id(owner, base) != u"tabsceneGameDetails" || index > 1)) return;
    call.r3.u64 = REX_LOAD_U32(owner); sub_8276C0F8(call, base);
    call.r4.u64 = REX_LOAD_U32(0x82D356C0); sub_82779A50(call, base);
    if (!Range(call.r3.u32, 24) || REX_LOAD_U32(call.r3.u32 + 8) <= index) return;
    // Consume this pointer motion once, so a resting cursor cannot repeatedly
    // restart the tab transition or reclaim focus from keyboard navigation.
    last_motion = pointer.motion;
    pdz_input::PointerAccept(pointer.click, 0);
    call.r4.u64 = index; call.r5.u64 = 1; sub_82792F10(call, base);
    return;
  }
  if ((postgame || owner_lobby) && id.size() == 7 && id.starts_with(u"Button") &&
      id[6] >= u'1' && id[6] <= u'6') return;
  // Footer parent buttons render first and would consume the click as A
  // before their labelled child can dispatch Start/LT/B/X.
  if (multiplayer && (id == u"buttonCreate" || id == u"buttonPlayerlist" ||
      id == u"buttonBack" || id == u"buttonBack1" || id == u"BackButton" ||
      id == u"buttonCustomGuns" || id == u"buttonFilterLocal" ||
      id == u"buttonFilterRemote" || id == u"A button")) return;
  // Multiplayer footer legends are visual children, not focusable controls.
  // Invoke their native pad action without moving focus off the selected row.
  const uint16_t multiplayer_action = id == u"A_Button" ? 0x5800 :
      id == u"B_Button" ? 0x5801 : id == u"X_Button" ? 0x5802 :
      id == u"Start_button" ? 0x5814 : id == u"L_TRIGGER" ? 0x5806 :
      id == u"R_TRIGGER" ? 0x5807 : 0;
  if (multiplayer && multiplayer_action) {
    if (!pointer.click) return;
    auto f = [base](uint32_t p) { return std::bit_cast<float>(REX_LOAD_U32(p)); };
    const float width = f(element + 12), height = f(element + 16);
    const float x = pointer.x * 1280.f - f(element + 208);
    const float y = pointer.y * 720.f - f(element + 212);
    if (!std::isfinite(x) || !std::isfinite(y) ||
        x < std::max(0.f, width - (postgame ? 165.f : 130.f)) || x > width ||
        y < 0 || y > std::min(height, 40.f)) return;
    pdz_input::PointerAccept(pointer.click, multiplayer_action);
    return;
  }
  trace.stage=4;
  const bool list_item = id == u"control_ListItem";
  uint32_t list_handle = 0;
  int32_t list_index = -1;
  if (list_item) {
    const auto parent = REX_LOAD_U32(element + 40);
    if (!Range(parent, 44)) return;
    call.r3.u64 = REX_LOAD_U32(parent); sub_8276C0F8(call, base);
    list_handle = call.r3.u32;
    call.r4.u64 = REX_LOAD_U32(0x82D356B4); // Registered XuiList.
    sub_82779A50(call, base);
    if (!Range(call.r3.u32, 92)) return;
    const auto count = REX_LOAD_U32(call.r3.u32 + 88);
    call.r3.u64 = handle; sub_8277A0E8(call, base); // Authored/virtualized row index.
    list_index = call.r3.s32;
    if (list_index < 0 || uint32_t(list_index) >= count) return;
  }
  // Scenes and containers can answer WANT_FOCUS too. Only actual buttons
  // should receive pointer focus; focusing a scene steals its children's input.
  call.r3.u64 = handle; call.r4.u64 = REX_LOAD_U32(0x82D356EC);
  sub_82779A50(call, base); // registered XuiButton class (includes NavButton)
  const bool value_control = multiplayer &&
      (id == u"DarkXuiValueButton" || id == u"DarkXuiValueButton_simple");
  if (!call.r3.u32 && !list_item && !value_control) return;
  // Value-row arrows are decorative text children, so native button hit
  // testing alone turns them into A presses instead of left/right changes.
  uint16_t arrow_key = 0;
  if (pointer.click && !list_item) {
    uint32_t pending[128]{}, seen[128]{};
    unsigned count = 0, visited = 0;
    pending[count++] = REX_LOAD_U32(element + 24);
    while (count && visited < 128) {
      const uint32_t child = pending[--count];
      if (!child || !Range(child, 240) ||
          std::find(seen, seen + visited, child) != seen + visited) continue;
      seen[visited++] = child;
      const auto child_id = Id(child, base);
      const uint32_t parent = REX_LOAD_U32(child + 40);
      const auto parent_id = Id(parent, base);
      if ((child_id == u"XuiText1" || child_id == u"XuiText2") &&
          parent_id == u"DarkXuiValueButtonSelected" && Visible(child, base)) {
        auto f = [base](uint32_t p) { return std::bit_cast<float>(REX_LOAD_U32(p)); };
        const float x = pointer.x * 1280.f - f(child + 208);
        const float y = pointer.y * 720.f - f(child + 212);
        if (std::isfinite(x) && std::isfinite(y) && x >= -5.f &&
            x <= f(child + 12) + 5.f && y >= 0 && y <= f(child + 16)) {
          arrow_key = child_id == u"XuiText1" ? 0x5812 : 0x5813;
          break;
        }
      }
      // +24 is the tail child; +32 walks from that tail toward its siblings.
      // +36 points back toward the tail and immediately ends at the first item.
      for (unsigned offset : {24u, 32u})
        if (count < 128) pending[count++] = REX_LOAD_U32(child + offset);
    }
  }
  trace.stage=5;
  call.r3.u64 = list_item ? list_handle : handle; call.r4.u64 = scratch;
  REX_STORE_U32(scratch, 0);
  sub_8277DD88(call, base); // native XM_WANT_FOCUS; excludes disabled controls
  if (call.r3.s32 < 0 || !REX_LOAD_U32(scratch)) return;
  if (arrow_key) {
    // Selected arrows belong to an already editing row. Setting focus again
    // sends its reset notification (2021), clearing the native editing flag
    // before the queued directional key can change the value.
    trace.stage=11;
    last_motion = pointer.motion;
    pdz_input::PointerAccept(pointer.click, arrow_key);
    return;
  }
  trace.stage=6;
  // Native hit testing uses the element's animated world transform and bounds.
  const uint32_t message = scratch + 32, payload = scratch + 64;
  REX_STORE_U32(message, 20); REX_STORE_U32(message + 4, 3);
  REX_STORE_U32(message + 8, 0); REX_STORE_U32(message + 12, 12);
  REX_STORE_U32(message + 16, payload);
  REX_STORE_U32(payload, uint32_t(pointer.x * 1280.f));
  REX_STORE_U32(payload + 4, uint32_t(pointer.y * 720.f));
  REX_STORE_U32(payload + 8, 0);
  call.r3.u64 = element; call.r4.u64 = message;
  sub_8278C2A0(call, base);
  if (!REX_LOAD_U32(payload + 8)) return;
  trace.stage=7;
  last_motion = pointer.motion;

  if (list_item) {
    call.r3.u64 = list_handle; call.r4.u64 = list_index;
    sub_8278ACA0(call, base); // Native selection updates details and row highlight.
    if (call.r3.s32 < 0) return;
    call.r3.u64 = list_handle; call.r4.u64 = focus_user; call.r5.u64 = 1;
    sub_82780C40(call, base); // Focus the selected child using native list rules.
    if (call.r3.s32 >= 0) pdz_input::PointerAccept(pointer.click);
    return;
  }
  if (focused_handle == handle) {
    pdz_input::PointerAccept(pointer.click);
    return;
  }
  call.r3.u64 = handle; call.r4.u64 = focus_user;
  sub_8277F5C8(call, base); // XuiElementSetUserFocus
  if (call.r3.s32 >= 0) pdz_input::PointerAccept(pointer.click);
  trace.stage=call.r3.s32>=0?8:9;
}
