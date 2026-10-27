#include "pd_menu_state.h"
#include "wheel_bindings.h"
#include <imgui.h>
#include <rex/ui/keybinds.h>
#include <sstream>
// TU0 camera addresses and angle/zoom conversion adapted from:
// marinesciencedude/xenia-canary-mousehook, PerfectDarkZero.cc.
// Copyright 2023 Ben Vanik. BSD-3-Clause; see LICENSE.mousehook.
#include "mousehook.h"
#include "game_cvars.h"
#include "game_patches.h"
#include "surface_runtime.h"
#include "cutscene_timing.h"
#include "input_logic.h"
#include "lockpick_mouse.h"
#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include "weapon_rules.h"
#include "coop_ally.h"
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstring>
#include <mutex>
#include <numbers>
#include <thread>
#include <deque>
#include <array>

namespace pdz_input {
namespace {
std::atomic<bool> supported{false};
std::atomic<bool> gameplay_active{true};
std::atomic<bool> menu_active{true};
std::atomic<uint32_t> local_inventory{0};
std::atomic<uint32_t> aim_pose_player{0};
std::atomic<int> inventory_page{0}, inventory_pages{1};
std::atomic<int64_t> inventory_render_time{0};
std::atomic<bool> crosshair_visible{false};
std::mutex lockpick_mutex;
LockpickMouse lockpick_mouse;
int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count(); }

// Check every page before following guest pointers, including level transitions.
uint8_t* Access(uint32_t address, uint32_t size, bool write = false) {
  auto* rt = rex::Runtime::instance();
  auto* memory = rt ? rt->memory() : nullptr;
  if (!memory || !address || !size || uint64_t(address) + size > 0x100000000ull)
    return nullptr;
  const uint64_t end = uint64_t(address) + size;
  for (uint64_t p = address; p < end;) {
    auto* heap = memory->LookupHeap(uint32_t(p));
    uint32_t protection = 0;
    const uint32_t required = rex::memory::kMemoryProtectRead |
        (write ? rex::memory::kMemoryProtectWrite : 0);
    if (!heap || !heap->QueryProtect(uint32_t(p), &protection) ||
        (protection & required) != required) return nullptr;
    p = (p / heap->page_size() + 1) * heap->page_size();
  }
  return memory->TranslateVirtual<uint8_t*>(address);
}
uint32_t Read32(uint32_t address) {
  auto* p = Access(address, 4);
  return p ? (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
      (uint32_t(p[2]) << 8) | p[3] : 0;
}
uint8_t Read8(uint32_t address) {
  auto* p = Access(address, 1);
  return p ? *p : 0;
}
uint32_t LockpickInteraction(uint32_t player) {
  if (!player || !Access(player,4084)) return 0;
  for (const auto offset : {4076u,4080u}) {
    const auto tool=Read32(player+offset);
    // Verified TU3 Locktopus and its live minigame, not other gadgets or menus.
    if (!Access(tool,1380) || Read32(tool)!=0x820C7038 ||
        Read32(tool+92)!=player || Read32(tool+1376)!=11) continue;
    const auto mini=Read32(tool+1316);
    if (Access(mini,160) && Read32(mini)==0x820CD850 &&
        Read32(mini+4)==tool && Read32(mini+32)==1) return mini;
  }
  return 0;
}
uint32_t CameraCamSpy(uint32_t camera) {
  if (!camera || !Access(camera, 24)) return 0;
  const auto object = Read32(camera + 16);
  // TU0 DP_CamSpy: native constructor 8252EE80; camera setup 82530FD0.
  return object && Access(object, 1574) && Read32(object) == 0x820BFA40 &&
      Read32(object + 12) == 50 && Read32(object + 1560) == Read32(camera + 20) &&
      Read8(object + 1571) ? object : 0;
}
uint32_t InputCamera() {
  const auto player = Read32(0x82D2B758);
  // CamSpy temporarily unregisters Joanna's camera and uses another viewport
  // slot. Follow only the device controlled by this local player.
  for (unsigned i=0;i<4;++i) {
    const auto camera = Read32(0x82D69048+i*4);
    const auto camspy = CameraCamSpy(camera);
    if (camspy && player && Read32(camspy+1536)==player) return camera;
  }
  return Read32(0x82D69048);
}
uint32_t InputCameraMode(uint32_t camera) {
  const auto camspy = CameraCamSpy(camera);
  return camspy ? Read32(camspy+1560) :
      (camera && Access(camera,0x540) ? Read32(camera+0x53C) : 0);
}
bool CameraLocked(uint32_t player, uint32_t camera, uint32_t mode) {
  if (!player || !mode) return true;
  if (!CameraCamSpy(camera)) return Read8(player+0x16A3)!=0;
  const auto session = Read32(0x82D6BFD8);
  return !session || !Access(session,1) || Read8(session)!=0;
}
uint32_t CameraPlayer(uint32_t camera) {
  // 82E35468 is the head of a controller list, including arena bots. The
  // first viewport's character reference is the actual mouse-controlled player.
  const auto valid = [](uint32_t p) {
    return p && Access(p, 0x16D2) && Read32(p + 12) == 2;
  };
  const uint32_t attached = camera && Access(camera, 20) ? Read32(camera + 8) : 0;
  if (valid(attached)) return attached;
  const auto camspy = CameraCamSpy(camera);
  const auto owner = camspy ? Read32(camspy + 1536) : 0;
  if (valid(owner)) return owner;
  const uint32_t player = Read32(0x82D2B758);
  return valid(player) ? player : 0;
}
float ReadFloat(uint32_t address) { return std::bit_cast<float>(Read32(address)); }
uint16_t Read16(uint32_t address) {
  auto* p = Access(address, 2);
  return p ? (uint16_t(p[0]) << 8) | p[1] : 0;
}
void UpdateInventory(uint32_t player) {
  // Retail constructors: sub_824F5698 sets weapon capacity at +0xA to 4;
  // sub_824F65B8 allocates 13 eight-byte nodes. Pickup sub_82586238 compares
  // capacity at +0xA with used size at +0x10. Keep the actual node bound.
  const uint32_t inventory = player ? player + 0x142C : 0;
  local_inventory = inventory;
  static uint32_t previous = 0;
  static uint16_t written = 4;
  if (!inventory || !Access(inventory, 0x56, true)) return;
  auto* cap = Access(inventory + 0xA, 2, true);
  const uint16_t current = Read16(inventory + 0xA);
  if (previous != inventory) { previous = inventory; written = 4; }
  if (current != 4 && current != written) return;
  const uint32_t storage = Read32(inventory + 0x30);
  const uint32_t free = Read32(inventory + 0x20);
  const uint32_t count = Read32(inventory + 0x2C);
  const uint16_t used = Read16(inventory + 0x10);
  if (!storage || !Access(storage, 13 * 8) || count > 13 || used > 39 ||
      (free && (free < storage || uint64_t(free) >= uint64_t(storage) + 13 * 8 ||
                (free - storage) % 8))) return;
  written = REXCVAR_GET(pdz_ignore_weapon_size) ? uint16_t(used + (free ? 3 : 0)) : 4;
  cap[0] = uint8_t(written >> 8); cap[1] = uint8_t(written);
}
void WriteFloat(uint8_t* p, float value) {
  const auto bits = std::bit_cast<uint32_t>(value);
  p[0] = uint8_t(bits >> 24); p[1] = uint8_t(bits >> 16);
  p[2] = uint8_t(bits >> 8); p[3] = uint8_t(bits);
}
void AddStick(uint8_t* p, float delta) {
  int value = int16_t((uint16_t(p[0]) << 8) | p[1]);
  value = std::clamp(value + int(std::clamp(delta, -32767.f, 32767.f)), -32768, 32767);
  p[0] = uint8_t(uint16_t(value) >> 8); p[1] = uint8_t(value);
}

class MouseListener final : public rex::ui::WindowInputListener,
                            public rex::ui::WindowListener {
 public:
  rex::ui::Window* window = nullptr;
  std::mutex mutex;
  float dx = 0, dy = 0;
  std::atomic<bool> focused{false};
  bool keys[256]{};
  uint16_t side_buttons = 0;
  bool right_mouse = false;
  bool pointer_held = false;
  float menu_x = 0, menu_y = 0;
  ButtonPulses pulses;
  bool aiming=false;
  std::deque<std::pair<uint16_t, uint16_t>> menu_keys;
  bool title_start_pending = true;
  float pointer_x = -1, pointer_y = -1;
  uint64_t pointer_motion = 0, pointer_click = 0, accepted_click = 0;
  int64_t pointer_click_time = 0;
  float click_x = -1, click_y = -1;
  void Position(const rex::ui::MouseEvent& e) {
    if (!window) return;
    pointer_x = float(e.x()) / std::max(1u, window->GetActualPhysicalWidth());
    pointer_y = float(e.y()) / std::max(1u, window->GetActualPhysicalHeight());
    ++pointer_motion;
  }
  void MenuKey(uint16_t vk) {
    if (menu_keys.size() > 30) return;
    menu_keys.emplace_back(vk, 1);
    menu_keys.emplace_back(vk, 2);
  }
  void Clear() {
    std::lock_guard lock(mutex); dx = dy = 0;
    std::fill(std::begin(keys), std::end(keys), false);
    pdz_weapons::ClearInspection();
    side_buttons = 0; right_mouse = false; menu_x = menu_y = 0; pulses.Clear(); menu_keys.clear();
    accepted_click = pointer_click; pointer_x = pointer_y = -1;
    pointer_held = false;
  }
  bool Active() const {
    return Supported() && gameplay_active && focused && REXCVAR_GET(pdz_mousehook) &&
        rex::cvar::GetFlagByName("mnk_mode") == "true";
  }
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    if (!Active()) return;
    std::lock_guard lock(mutex);
    auto key = uint32_t(e.virtual_key());
    // Handle the gameplay camera key here, before lower-priority UI listeners.
    // Track its edge explicitly so keyboard auto-repeat cannot toggle it twice.
    const auto cameraKey=uint32_t(rex::ui::ParseVirtualKey(rex::cvar::GetFlagByName("bind_third_person")));
    if(key && key<256 && key==cameraKey && !menu_active){
      if(!keys[key])REXCVAR_SET(pdz_tp_enable,!REXCVAR_GET(pdz_tp_enable));
      keys[key]=true;e.set_handled(true);return;
    }
    if (key == 'I' && !menu_active && crosshair_visible) {
      if (!keys[key]) {
        const auto player = CameraPlayer(InputCamera());
        const auto right = Read32(player + 4076);
        const auto left = Read32(player + 4080);
        const bool dual = Read32(player + 4088) == 2;
        pdz_weapons::RequestInspection(right ? right : left, dual && right ? left : 0);
      }
      keys[key] = true;
      e.set_handled(true);
      return;
    }
    if(pdz_surface::IntroCameraActive() && (key==0x0D || key==0x20)){
      pdz_surface::RequestIntroTitleSkip();e.set_handled(true);return;
    }
    if (menu_active && (key == 'A' || key == 'D')) {
      MenuKey(key == 'A' ? 0x5812 : 0x5813);
      e.set_handled(true);
      return;
    }
    if (menu_active && key == 'E') {
      MenuKey(0x5800);
      e.set_handled(true);
      return;
    }
      if (menu_active && key == 'X') {
        MenuKey(0x5802);
        e.set_handled(true);
        return;
      }
    if (key < 256) keys[key] = true;
    if (menu_active && key == 0x21) ChangeInventoryPage(-1);
    if (menu_active && key == 0x22) ChangeInventoryPage(1);
    if (menu_active && (key == uint32_t(rex::ui::VirtualKey::kReturn))) title_start_pending = false;
  }
  void OnKeyUp(rex::ui::KeyEvent& e) override {
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    std::lock_guard lock(mutex);
    auto key = uint32_t(e.virtual_key());
    if (key < 256) keys[key] = false;
  }
  void OnMouseDown(rex::ui::MouseEvent& e) override {
    if(pdz_menu::Overlay() && e.button()==rex::ui::MouseEvent::Button::kMiddle && ImGui::GetCurrentContext())ImGui::GetIO().AddMouseButtonEvent(2,true);
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    if (!Active()) return;
    std::lock_guard lock(mutex);
    if (menu_active) {
      Position(e);
      using B = rex::ui::MouseEvent::Button;
      if (e.button() == B::kLeft) {
        pointer_held = true;
        if (title_start_pending) MenuKey(0x5814);
        else {
          ++pointer_click;
          pointer_click_time = NowMs();
          click_x = pointer_x; click_y = pointer_y;
        }
        title_start_pending = false;
      }
      else if (e.button() == B::kRight) MenuKey(0x5801);
      else if (e.button() == B::kMiddle) { MenuKey(0x5814); title_start_pending = false; }
      else if (e.button() == B::kX1) MenuKey(0x5805);
      else if (e.button() == B::kX2) MenuKey(0x5804);
      e.set_handled(true);
      return;
    }
    if (e.button() == rex::ui::MouseEvent::Button::kRight) {
      right_mouse = true;
      e.set_handled(true);
    }
    if (e.button() == rex::ui::MouseEvent::Button::kX1) side_buttons |= 0x8000;
    if (e.button() == rex::ui::MouseEvent::Button::kX2) side_buttons |= 0x0100;
  }
  void OnMouseUp(rex::ui::MouseEvent& e) override {
    if(e.button()==rex::ui::MouseEvent::Button::kMiddle && ImGui::GetCurrentContext())ImGui::GetIO().AddMouseButtonEvent(2,false);
    if (e.button() == rex::ui::MouseEvent::Button::kLeft) {
      std::lock_guard lock(mutex);
      pointer_held = false;
      if (menu_active) Position(e);
    }
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    std::lock_guard lock(mutex);
    if (e.button() == rex::ui::MouseEvent::Button::kRight) right_mouse = false;
    if (e.button() == rex::ui::MouseEvent::Button::kX1) side_buttons &= ~0x8000;
    if (e.button() == rex::ui::MouseEvent::Button::kX2) side_buttons &= ~0x0100;
  }
  void OnMouseWheel(rex::ui::MouseEvent& e) override {
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    if (!Active() || !e.scroll_y()) return;
    std::lock_guard lock(mutex);
    const int detents = std::clamp(e.scroll_y() / 120, -16, 16);
    if (!menu_active && (right_mouse || aiming)) {
      pulses.Clear();
      REXCVAR_SET(pdz_ads_pressure, AimPressure(REXCVAR_GET(pdz_ads_pressure), detents));
    } else {
      uint16_t button=0;
      if(!menu_active)for(auto binding:wheel_buttons)if(HasWheel(rex::cvar::GetFlagByName(binding.name),detents>0?"WheelUp":"WheelDown"))button|=binding.mask;
      for (int i = 0; i < std::abs(detents); ++i) {
        if (menu_active) {
          using VK = rex::ui::VirtualKey;
          const bool shift = keys[uint32_t(VK::kShift)] || keys[uint32_t(VK::kLShift)] || keys[uint32_t(VK::kRShift)];
          const bool ctrl = keys[uint32_t(VK::kControl)] || keys[uint32_t(VK::kLControl)] || keys[uint32_t(VK::kRControl)];
          MenuKey(ctrl ? (detents > 0 ? 0x5805 : 0x5804) :
                  shift ? (detents > 0 ? 0x5812 : 0x5813) :
                          (detents > 0 ? 0x5810 : 0x5811));
        }
        else if(button)pulses.Push(button);
      }
    }
    e.set_handled(true);
  }
  void OnLostFocus(rex::ui::UISetupEvent&) override { focused = false; Clear(); }
  void OnGotFocus(rex::ui::UISetupEvent&) override { Clear(); focused = true; }
  void OnClosing(rex::ui::UIEvent&) override { Detach(); }
  void OnMouseMove(rex::ui::MouseEvent& e) override {
    if (pdz_menu::Overlay()) { e.set_handled(true); return; }
    if (!Supported() || !gameplay_active || !REXCVAR_GET(pdz_mousehook) || !focused ||
        (!menu_active && !rex::input::mnk::IsMouseLookActive())) { Clear(); return; }
    // Keep SDK capture and button handling, but consume motion ahead of its
    // stick emulation. ImGui (z=64) still receives the event first.
    if (rex::cvar::GetFlagByName("mnk_mode") != "true" ||
        rex::cvar::GetFlagByName("mnk_mouse") != "true") { Clear(); return; }
    std::lock_guard lock(mutex);
    if (menu_active) {
      Position(e);
      e.set_handled(true);
      return;
    }
    dx = std::clamp(dx + e.dx(), -10000.f, 10000.f);
    dy = std::clamp(dy + e.dy(), -10000.f, 10000.f);
    e.set_handled(true);
  }
} listener;
}

Pointer MenuPointer() {
  std::lock_guard lock(listener.mutex);
  const bool click = listener.pointer_click > listener.accepted_click &&
      NowMs() - listener.pointer_click_time < 300 &&
      std::abs(listener.pointer_x - listener.click_x) < .01f &&
      std::abs(listener.pointer_y - listener.click_y) < .01f;
  return {listener.pointer_x, listener.pointer_y, listener.pointer_motion,
      click ? listener.pointer_click : 0,
      listener.Active() && menu_active && listener.pointer_x >= 0,
      listener.pointer_held, listener.pointer_click, listener.click_x, listener.click_y,
      listener.pointer_click > listener.accepted_click};
}
void PointerAccept(uint64_t click, uint16_t key) {
  if (!click) return;
  std::lock_guard lock(listener.mutex);
  if (click <= listener.accepted_click) return;
  listener.accepted_click = click;
  if (key) listener.MenuKey(key);
}
void InventoryMenuRendered() { inventory_render_time = NowMs(); }
void PointerDialogAccept(uint64_t click, uint16_t key) {
  if (!click || (key != 0x5800 && key != 0x5801)) return;
  std::lock_guard lock(listener.mutex);
  if (click <= listener.accepted_click || !menu_active) return;
  listener.accepted_click = click;
  // Notification prompts poll the pad packet rather than the focused XUI
  // keystroke queue. Supply one timed press, with no duplicate menu event.
  listener.pulses.Push(key == 0x5800 ? 0x1000 : 0x2000);
}
bool CrosshairVisible() { return crosshair_visible && gameplay_active && listener.focused; }
bool WeatherVisible(bool cinematic) {
  // Cover and traversal hide the reticle without hiding the world. Check
  // menus independently, including on render frames where simulation is paused.
  if (!Supported() || (!gameplay_active && !cinematic) || REXCVAR_GET(pdz_menu_mode) ||
      NowMs() - inventory_render_time < 150) return false;
  const uint32_t session = Read32(0x82D6BFD8);
  return session && Access(session, 1) && !Read8(session);
}

int InventoryPage() { return inventory_page.load() + 1; }
int InventoryPages() { return inventory_pages.load(); }
void ChangeInventoryPage(int direction) {
  if (!REXCVAR_GET(pdz_ignore_weapon_size)) return;
  const int pages = inventory_pages.load();
  inventory_page = (inventory_page.load() + direction + pages) % pages;
}
bool InventoryPageItem(uint32_t ui, uint32_t index, uint32_t& item) {
  if (!Supported() || !Access(ui, 40)) return false;
  // InventorySlotButton: live inventory mode, owner at +36, player at +952.
  const uint32_t owner = Read32(ui + 36);
  const uint32_t player = owner && Access(owner, 968) ? Read32(owner + 964) : 0;
  if (!player || !Access(player, 0x1482)) return false;
  if (!REXCVAR_GET(pdz_ignore_weapon_size) || Read8(ui + 32) ||
      player + 0x142C != local_inventory) return false;
  const uint32_t inv = player + 0x142C;
  const uint32_t list = Read32(inv);
  const uint32_t storage = Read32(inv + 0x30);
  if (!Access(list, 12) || !Access(storage, 104)) return false;
  const uint32_t nodes = Read32(list), head = Read32(list + 8);
  if (!Access(nodes, 256) || head < nodes || uint64_t(head) >= uint64_t(nodes) + 256 || (head - nodes) % 8) return false;
  std::array<uint32_t, 13> ids{};
  std::array<int, 13> sizes{};
  uint32_t visited = 0;
  int count = 0, n = int((head - nodes) / 8);
  while (n != -1 && count < 13) {
    if (n < 0 || n >= 32 || (visited & (1u << n))) return false;
    visited |= 1u << n;
    const uint32_t node = nodes + n * 8;
    const uint32_t data = Read32(node + 4);
    if (data < storage || uint64_t(data) >= uint64_t(storage) + 104 || (data - storage) % 8) return false;
    if (Read32(data + 4) == 1) {
      const uint32_t weapon = Read32(data);
      if (!Access(weapon, 864)) return false;
      const uint32_t def = Read32(weapon + 860);
      if (!Access(def, 1148)) return false;
      ids[count] = Read32(def + 1144);
      if (!ids[count] || ids[count] > 62) return false;
      sizes[count++] = std::min(3, int(Read8(def + 200)) + 1);
    }
    n = int16_t(Read16(node + 2));
  }
  const auto layout = PaginateInventory(sizes, count);
  const auto& starts = layout.starts;
  const int pages = layout.pages;
  static std::mutex page_mutex;
  std::lock_guard lock(page_mutex);
  static uint32_t previous_player = 0;
  static int previous_count = 0;
  if (previous_player != player) { inventory_page = 0; previous_count = count; previous_player = player; }
  else if (count > previous_count) inventory_page = pages - 1;
  previous_count = count;
  inventory_pages = pages;
  const int page = std::clamp(inventory_page.load(), 0, pages - 1);
  inventory_page = page;
  const uint64_t actual = uint64_t(starts[page]) + index;
  item = actual < uint64_t(starts[page + 1]) ? ids[size_t(actual)] : 0;
  return true;
}
bool MenuKeystroke(uint32_t user, uint32_t address) {
  if (user != 0 && (user & 0xFF) != 0xFF) return false;
  if (!listener.Active() || !menu_active) return false;
  auto* out = Access(address, 8, true);
  if (!out) return false;
  std::lock_guard lock(listener.mutex);
  if (listener.menu_keys.empty()) return false;
  const auto [key, flags] = listener.menu_keys.front();
  listener.menu_keys.pop_front();
  std::memset(out, 0, 8);
  out[0] = uint8_t(key >> 8); out[1] = uint8_t(key);
  out[4] = uint8_t(flags >> 8); out[5] = uint8_t(flags);
  return true;
}
void PaceFrame() {
  // Presentation-thread pacing, independent of input polls (cutscenes may
  // stop polling). Reset the deadline as soon as normal gameplay resumes.
  static thread_local std::chrono::steady_clock::time_point previous{};
  const uint32_t camera = InputCamera();
  const uint32_t player = CameraPlayer(camera);
  const uint32_t mode_ptr = InputCameraMode(camera);
  const bool valid = player && Access(player, 0x16D2);
  const uint8_t mode = CameraCamSpy(camera) ? 5 :
      (mode_ptr && Access(mode_ptr, 12) ? Read8(mode_ptr + 11) : 255);
  const bool normal = mode == 0 || mode == 1 || mode == 2 || mode == 3 || mode == 4 || mode == 6 ||
      (mode == 5 && CameraCamSpy(camera));
  const bool cap = Supported() && REXCVAR_GET(pdz_cutscene_60fps) &&
      (!valid || !normal || CameraLocked(player,camera,mode_ptr));
  if (!cap) { previous = {}; return; }
  const auto interval = std::chrono::nanoseconds(16666667);
  if (previous != std::chrono::steady_clock::time_point{})
    std::this_thread::sleep_until(previous + interval);
  previous = std::chrono::steady_clock::now();
}
bool ForceAimPose(uint32_t player) {
  return Supported() && gameplay_active && listener.focused &&
      REXCVAR_GET(pdz_mousehook) && REXCVAR_GET(pdz_all_weapon_ads) && REXCVAR_GET(pdz_wheel_ads) &&
      REXCVAR_GET(pdz_ads_pressure) == 32 && player && player == aim_pose_player &&
      Access(player, 0x1914) && Read32(player + 4088) != 2 && !Read8(player + 0x16A3);
}
bool Supported() { return supported.load(); }
bool InventoryStorageFull(uint32_t inventory) {
  return Supported() && REXCVAR_GET(pdz_ignore_weapon_size) && inventory &&
      inventory == local_inventory && Access(inventory, 0x34) &&
      (Read32(inventory + 0x20) == 0 || Read32(inventory + 0x2C) >= 13);
}
void SetGameplayInputActive(bool active) {
  gameplay_active = active;
  rex::input::mnk::SetMouseLookActive(active && !menu_active);
  if (!active) { aim_pose_player = 0; listener.Clear(); }
}
void ValidateImage() {
  constexpr char build[] = "19.09.06.0082";
  auto* signature = Access(0x820CD9E0, sizeof(build));
  supported = signature && std::memcmp(signature, build, sizeof(build)) == 0 &&
      ReadFloat(0x820EAF40) == 58.f;
  REXLOG_INFO("PDZ direct mouse/FOV TU3 validation: {}",
              Supported() ? "matched" : "unsupported (disabled)");
}
void Attach(rex::ui::Window* window) {
  Detach();
  listener.window = window;
  if (window) {
    listener.focused = window->HasFocus();
    window->AddListener(&listener);
    window->AddInputListener(&listener, 32);
  }
}
void Detach() {
  if (auto* window = listener.window) {
    window->RemoveInputListener(&listener);
    window->RemoveListener(&listener);
    listener.window = nullptr;
  }
  listener.focused = false;
  listener.Clear();
}

void Poll(uint8_t* state) {
  if (!Supported()) return;
  static std::mutex poll_mutex;
  std::lock_guard poll_lock(poll_mutex);
  // Refresh context even when the mouse is stationary. Never carry camera
  // deltas across scene/camera changes (notably River Extraction's ending).
  const uint32_t camera = InputCamera();
  const uint32_t player = CameraPlayer(camera);
  UpdateInventory(player);
  const uint32_t body = player ? Read32(player + 0xE4) : 0;
  const uint32_t session = Read32(0x82D6BFD8);
  const bool paused = session && Access(session, 1) && Read8(session);
  pdz_coop::PollRescueInput(state && !paused && (state[4]&0x10));
  if(pdz_coop::PlayerDowned() && !paused){
    if(state){const uint8_t start=state[5]&0x10;
      std::memset(state+4,0,12);state[5]=start;}
    crosshair_visible=false;aim_pose_player=0;listener.Clear();return;
  }
  static bool intro_accept_down=false;
  const bool accept_down=state && (state[4]&0x10); // Native A button.
  if(pdz_surface::IntroCameraActive() && !paused && accept_down && !intro_accept_down)
    pdz_surface::RequestIntroTitleSkip();
  intro_accept_down=accept_down;
  // Apply before mouse-mode early returns: this is the shared native pad
  // packet for both controllers and keyboard/mouse. Preserve Start to pause.
  if(pdz_surface::IntroCameraActive() && !paused){
    if(state){const uint8_t start=state[5]&0x10;
      std::memset(state+4,0,12);state[5]=start;}
    crosshair_visible=false;aim_pose_player=0;listener.Clear();return;
  }
  const bool menu = REXCVAR_GET(pdz_mouse_menu_navigation) &&
      (REXCVAR_GET(pdz_menu_mode) || paused || NowMs() - inventory_render_time < 150 || !player || !body);
  if (menu_active.exchange(menu) != menu) listener.Clear();
  // Release the SDK pointer lock in menus so Windows can display the cursor.
  rex::input::mnk::SetMouseLookActive(!menu && gameplay_active);
  const uint32_t mode_ptr = InputCameraMode(camera);
  const uint8_t mode = CameraCamSpy(camera) ? 5 :
      (mode_ptr && Access(mode_ptr, 12) ? Read8(mode_ptr + 0xB) : 0);
  const bool locked = CameraLocked(player,camera,mode_ptr);
  pdz_weapons::PollWallReload(player, state && !paused && !menu && !locked &&
      (mode == 0 || mode == 1) && (state[4] & 0x40));
  crosshair_visible = player && body && !locked && !menu && (mode == 0 || mode == 1);
  struct Context {
    uint32_t player, body, camera, mode_ptr;
    uint8_t mode;
    bool locked;
    bool operator==(const Context&) const = default;
  };
  static Context previous{};
  const Context current{player, body, camera, mode_ptr, mode, locked};
  const bool changed = !(current == previous);
  previous = current;
  float dx, dy;
  bool walk = false;
  float walkX=0,walkY=0;
  bool right_mouse = false;
  uint16_t buttons = 0;
  { std::lock_guard lock(listener.mutex);
    dx = changed ? 0.f : listener.dx; dy = changed ? 0.f : listener.dy;
    listener.dx = listener.dy = 0;
    using VK = rex::ui::VirtualKey;
    const auto down = [](VK key) {
      if(key==VK::kShift)return listener.keys[uint32_t(key)]||listener.keys[uint32_t(VK::kLShift)]||listener.keys[uint32_t(VK::kRShift)];
      if(key==VK::kControl)return listener.keys[uint32_t(key)]||listener.keys[uint32_t(VK::kLControl)]||listener.keys[uint32_t(VK::kRControl)];
      if(key==VK::kMenu)return listener.keys[uint32_t(key)]||listener.keys[uint32_t(VK::kLMenu)]||listener.keys[uint32_t(VK::kRMenu)];
      return listener.keys[uint32_t(key)];
    };
    {
      // Shift is a walking modifier, not an extra requirement on movement binds.
      const auto moving=[&](const char* name){
        std::istringstream aliases(rex::cvar::GetFlagByName(name));std::string token;
        while(std::getline(aliases,token,',')){
          token.erase(0,token.find_first_not_of(" "));auto end=token.find_last_not_of(" ");
          if(end==std::string::npos)continue;token.resize(end+1);
          bool ctrl=false,alt=false;
          while(token.find('+')!=std::string::npos){auto pos=token.find('+');auto mod=token.substr(0,pos);
            if(mod=="Ctrl"||mod=="Control")ctrl=true;else if(mod=="Alt")alt=true;else if(mod!="Shift")break;
            token.erase(0,pos+1);
          }
          if((ctrl && !(down(VK::kControl)||down(VK::kLControl)||down(VK::kRControl))) ||
             (alt && !(down(VK::kMenu)||down(VK::kLMenu)||down(VK::kRMenu))))continue;
          auto key=rex::ui::ParseVirtualKey(token);auto index=uint32_t(key);
          if(index>0 && index<256 && down(key))return true;
        }return false;
      };
      walk=moving("keybind_walk");
      walkX=float(moving("keybind_lstick_right"))-float(moving("keybind_lstick_left"));
      walkY=float(moving("keybind_lstick_up"))-float(moving("keybind_lstick_down"));
    }
    buttons = listener.side_buttons;
    right_mouse = listener.right_mouse;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    listener.aiming=SuppressWheelActions(right_mouse,state?state[6]:0);
    if(listener.aiming)listener.pulses.Clear();
    else buttons |= listener.pulses.Poll(now);
  }
  aim_pose_player = right_mouse && !menu && !locked && (mode == 0 || mode == 1) ? player : 0;
  const bool mouse_lockpick = state && gameplay_active && !menu && !paused &&
      listener.focused && REXCVAR_GET(pdz_mousehook) &&
      rex::cvar::GetFlagByName("mnk_mode")=="true" &&
      rex::cvar::GetFlagByName("mnk_mouse")=="true";
  const auto lockpick = mouse_lockpick ? LockpickInteraction(player) : 0;
  {
    std::lock_guard lock(lockpick_mutex);
    if(changed)lockpick_mouse.Reset();
    lockpick_mouse.Move(lockpick,dx,dy);
  }
  if(lockpick) {
    // The minigame consumes calibrated axes in its own update below. Suppress
    // movement keys and camera input while retaining native buttons/cancel.
    std::memset(state+8,0,8);
    return;
  }
  if (!Supported()) return;
  game_patches::UpdateFov();
  if (!state || !gameplay_active || !REXCVAR_GET(pdz_mousehook) || !listener.focused ||
      (!menu && !rex::input::mnk::IsMouseLookActive()) ||
      rex::cvar::GetFlagByName("mnk_mode") != "true") return;
  state[4] |= uint8_t(buttons >> 8); state[5] |= uint8_t(buttons);
  if (menu) return;
  if (REXCVAR_GET(pdz_wheel_ads)) {
    const bool dual = player && Access(player, 4092) && Read32(player + 4088) == 2;
    state[6] = MergeMouseAimTrigger(state[6], right_mouse, dual, REXCVAR_GET(pdz_ads_pressure));
  }
  if (walk) {
    const float lx = walkX*32767.f;
    const float ly = walkY*32767.f;
    const float length = std::hypot(lx, ly);
    const float limit = 32767.f * float(std::clamp(REXCVAR_GET(pdz_walk_speed),10.0,100.0)) / 100.f;
    if (length > 0) {
      // Native input calibration (sub_822733D0) reserves 6000 units around
      // centre. Encode walking above that range, including diagonal axes.
      const auto encode=[&](float axis){
        const float value=axis*std::min(1.f,limit/length)/32767.f;
        return uint16_t(int16_t(value==0 ? 0 : std::copysign(6000.f+26767.f*std::abs(value),value)));
      };
      const auto x=encode(lx);
      const auto y=encode(ly);
      state[8] = uint8_t(x >> 8); state[9] = uint8_t(x);
      state[10] = uint8_t(y >> 8); state[11] = uint8_t(y);
    }
  }
  if (rex::cvar::GetFlagByName("mnk_mouse") != "true" || (!dx && !dy)) return;

  if (!player || !body || locked) return;
  const bool vehicle = mode == 2 || mode == 4;
  const bool cover = mode == 3;
  const bool turret = mode == 6;
  const uint32_t camspy = mode == 5 ? CameraCamSpy(camera) : 0;
  const float sensitivity = float(REXCVAR_GET(pdz_mouse_sensitivity));
  // The old hook bypassed the control-lock flag for jetpack mode and followed
  // vehicle pointers during transitions. Fail closed for all locked cameras.
  if (!mode_ptr || locked) return;
  if (mode != 0 && mode != 1 && mode != 2 && mode != 3 && mode != 4 && mode != 6 && !camspy) return;
  if (!body || !Access(body, 0x154)) return;
  uint32_t x = cover ? player + 0x1670 : body + 0x150;
  uint32_t y = player + 0x1674;
  // CamSpy's native yaw/pitch virtual getters use degrees, unlike body yaw.
  if (camspy) { x = camspy + 1380; y = camspy + 1376; }
  const bool degrees_x = cover || camspy;
  bool radians_y = false;
  if (turret || vehicle) {
    const uint32_t object = Read32(player + 0x5C);
    if (!object || !Access(object, vehicle ? 0xE5C : 0x3A4)) return;
    if (turret && (object < 0x00100000 || object >= 0x20000000)) return;
    x = object + (vehicle ? 0xE58 : 0x3A0);
    y = object + (vehicle ? 0xE54 : 0x39C);
    radians_y = true;
  }
  auto* xp = Access(x, 4, true);
  auto* yp = Access(y, 4, true);
  if (!xp || !yp) return;
  constexpr float degrees = 180.f / std::numbers::pi_v<float>;
  float yaw = ReadFloat(x) * (degrees_x ? 1.f : degrees);
  float pitch = ReadFloat(y) * (radians_y ? degrees : 1.f);
  if (!std::isfinite(yaw) || !std::isfinite(pitch)) return;
  const float fov = ReadFloat(0x820EAF40);
  const float current_fov = !camspy && camera && Access(camera, 0x444) ? ReadFloat(camera + 0x440) : 0;
  float zoom = 1.f;
  if (std::isfinite(current_fov) && current_fov > 0.01f && current_fov <= 180.f)
    zoom = std::max(1.f, fov / current_fov);
  if (zoom >= 45.f) zoom /= 2.5f;
  if (zoom > 1.f) zoom = 0.9f * zoom + 0.1f * zoom * zoom * 1.1f;
  const float scale = sensitivity / (8.40517241378f * zoom * (fov / 58.f));
  yaw += dx * scale * (REXCVAR_GET(pdz_mouse_invert_x) ? 1.f : -1.f);
  pitch += dy * scale * (REXCVAR_GET(pdz_mouse_invert_y) ? -1.f : 1.f);
  yaw = cover ? std::clamp(yaw, -68.f, 68.f) : std::remainder(yaw, 360.f);
  pitch = std::clamp(pitch, -89.f, 89.f);
  WriteFloat(xp, yaw / (degrees_x ? 1.f : degrees));
  WriteFloat(yp, pitch / (radians_y ? degrees : 1.f));
}
}



REX_EXTERN(__imp__sub_826492E8);
void pdz_input::UpdateLockpick(PPCContext& ctx, uint8_t* base) {
  using namespace pdz_input;
  const auto mini=ctx.r3.u32;
  float x=0,y=0;
  bool apply=false;
  {
    std::lock_guard lock(lockpick_mutex);
    apply=mini && mini==lockpick_mouse.interaction && listener.focused &&
        gameplay_active && !menu_active && REXCVAR_GET(pdz_mousehook) &&
        rex::cvar::GetFlagByName("mnk_mode")=="true" &&
        rex::cvar::GetFlagByName("mnk_mouse")=="true";
    x=lockpick_mouse.x; y=lockpick_mouse.y;
  }
  const auto player=apply ? CameraPlayer(InputCamera()) : 0;
  if(!apply || LockpickInteraction(player)!=mini) {
    __imp__sub_826492E8(ctx,base); return;
  }
  // Native sub_826492E8 gets actor+6560 through sub_825A6048, then
  // reads calibrated axes at +1724/+1728. Override only this invocation.
  const auto owner=Read32(player+6560);
  auto* axes=Access(owner+1724,8,true);
  if(!owner || !axes) { __imp__sub_826492E8(ctx,base); return; }
  std::array<uint8_t,8> saved;
  std::memcpy(saved.data(),axes,8);
  WriteFloat(axes,x); WriteFloat(axes+4,y);
  __imp__sub_826492E8(ctx,base);
  std::memcpy(axes,saved.data(),8);
}
