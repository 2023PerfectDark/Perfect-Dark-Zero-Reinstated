from pathlib import Path
import shutil
root = Path(__file__).resolve().parent
repo = root / 'xenia-edge-mousehook'
dest = repo / 'src/xenia/hid/winkey/hookables'
dest.mkdir(parents=True, exist_ok=True)
for name in ('PerfectDarkZero.h', 'PerfectDarkZero.cc'):
    data = (root / 'mousehook-reference/src/xenia/hid/winkey/hookables' / name).read_text()
    data = data.replace('GameVersion title_version', 'uint32_t title_version')
    data = data.replace('#include "xenia/hid/hid_flags.h"', '#include "xenia/base/cvar.h"\n#include <algorithm>\n#include <cmath>\n#include <map>')
    (dest / name).write_text(data)
(dest / 'hookable_game.h').write_text('''// Compatibility interface for the original PDZ mousehook (BSD license).
#pragma once
#include <cstdint>
#include <string>
#include "xenia/base/byte_order.h"
#include "xenia/hid/input.h"
namespace xe::hid::winkey {
struct MouseEvent { int32_t x_delta=0, y_delta=0, buttons=0, wheel_delta=0; };
struct RawInputState { MouseEvent mouse; bool* key_states=nullptr; };
enum MidHookStatus : uint8_t { NOT_HOOKED, HOOKED };
class HookableGame {
 public:
  virtual ~HookableGame() = default;
  virtual bool IsGameSupported(uint32_t)=0;
  virtual bool DoHooks(uint32_t, RawInputState&, X_INPUT_STATE*)=0;
  virtual std::string ChooseBinds()=0;
  virtual bool ModifierKeyHandler(uint32_t, RawInputState&, X_INPUT_STATE*)=0;
  virtual void WeaponSwitchHandler(uint32_t, RawInputState&, X_INPUT_STATE*, int, uint16_t)=0;
  virtual void MidHookInit()=0;
  MidHookStatus midhook_status=NOT_HOOKED;
};
}
''')

# Raw deltas cross the UI/guest thread boundary atomically, with no key polling
# or memory modifications while focus/capture is released.
(repo / 'src/xenia/ui/pdz_raw_mouse.h').write_text('''#pragma once
#include <atomic>
#include <cstdint>
namespace xe::ui::pdz_mouse {
inline std::atomic<bool> captured{false};
inline std::atomic<int32_t> dx{0}, dy{0};
inline void Reset() { dx.store(0); dy.store(0); }
}
''')
window = repo / 'src/xenia/ui/window_wx.cc'
s = window.read_text()
s = s.replace('#include "xenia/ui/window_wx.h"', '#include "xenia/ui/window_wx.h"\n#include "xenia/ui/pdz_raw_mouse.h"')
s = s.replace('      case WM_PAINT:', '''      case WM_KEYDOWN:
        if (wParam == VK_F8 && !(lParam & (1LL << 30))) {
          const bool capture = !pdz_mouse::captured.load();
          pdz_mouse::captured.store(capture);
          pdz_mouse::Reset();
          if (capture) {
            RECT rect; GetClientRect(GetHWND(), &rect);
            MapWindowPoints(GetHWND(), nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
          } else { ClipCursor(nullptr); }
          SetCursor(capture ? nullptr : LoadCursor(nullptr, IDC_ARROW));
          return 0;
        }
        break;
      case WM_SETCURSOR:
        if (pdz_mouse::captured.load()) { SetCursor(nullptr); return TRUE; }
        break;
      case WM_KILLFOCUS:
        pdz_mouse::captured.store(false);
        pdz_mouse::Reset();
        ClipCursor(nullptr);
        break;
      case WM_INPUT: {
        RAWINPUT input{}; UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                            &input, &size, sizeof(RAWINPUTHEADER)) != UINT(-1) &&
            input.header.dwType == RIM_TYPEMOUSE &&
            !(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) &&
            pdz_mouse::captured.load()) {
          pdz_mouse::dx.fetch_add(input.data.mouse.lLastX);
          pdz_mouse::dy.fetch_add(input.data.mouse.lLastY);
        }
        break;
      }
      case WM_PAINT:''', 1)
s = s.replace('  render_panel->set_owner(this);', '''  render_panel->set_owner(this);
  RAWINPUTDEVICE mouse_device{0x01, 0x02, 0, render_panel->GetHWND()};
  if (!RegisterRawInputDevices(&mouse_device, 1, sizeof(mouse_device))) {
    XELOGW("PDZ mousehook: raw mouse registration failed: {}", GetLastError());
  }''')
window.write_text(s)

driver = repo / 'src/xenia/hid/keyboard/keyboard_input_driver.cc'
s = driver.read_text()
s = s.replace('#include "xenia/hid/keyboard/keyboard_input_driver.h"', '''#include "xenia/hid/keyboard/keyboard_input_driver.h"
#if XE_PLATFORM_WIN32
#include "xenia/hid/keyboard/pdz_mousehook.h"
#endif''')
s = s.replace('  return X_ERROR_SUCCESS;\n}\n\nX_RESULT KeyboardInputDriver::SetState', '''#if XE_PLATFORM_WIN32
  pdz_mousehook::Update(window(), user_index, out_state);
#endif
  return X_ERROR_SUCCESS;
}

X_RESULT KeyboardInputDriver::SetState''')
s = s.replace('info.display_name = "Keyboard";', 'info.display_name = "Keyboard + PDZ Mousehook";')
s = s.replace('info.auto_bind = false;', 'info.auto_bind = true;\n  info.preferred_slot = 0;')
driver.write_text(s)
(repo / 'src/xenia/hid/keyboard/pdz_mousehook.h').write_text('''#pragma once
#include "xenia/hid/input.h"
#include "xenia/ui/window.h"
namespace xe::hid::pdz_mousehook {
void Update(ui::Window* window, uint32_t user_index, X_INPUT_STATE* state);
}
''')
(repo / 'src/xenia/hid/keyboard/pdz_mousehook_win.cc').write_text('''// PDZ-only adapter for marinesciencedude's Canary mousehook.
#include "xenia/hid/keyboard/pdz_mousehook.h"
#include "xenia/base/platform_win.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/hid/winkey/hookables/PerfectDarkZero.h"
#include "xenia/ui/pdz_raw_mouse.h"
#include <mutex>

DEFINE_bool(pdz_mousehook, true, "Enable the PDZ mousehook; F8 captures/releases mouse", "MouseHook");
DEFINE_double(sensitivity, 1.0, "Mouse sensitivity", "MouseHook");
DEFINE_double(fov_sensitivity, 1.0, "Zoom sensitivity scaling", "MouseHook");
DEFINE_double(right_stick_hold_time_workaround, 33.0, "Special camera stick hold milliseconds", "MouseHook");
DEFINE_bool(invert_x, false, "Invert mouse X", "MouseHook");
DEFINE_bool(invert_y, false, "Invert mouse Y", "MouseHook");
DEFINE_bool(ge_gun_sway, true, "Enable native mousehook gun sway", "MouseHook");

namespace xe::hid::pdz_mousehook {
void Update(ui::Window* window, uint32_t user_index, X_INPUT_STATE* state) {
  static std::mutex mutex;
  std::lock_guard lock(mutex);
  using namespace ui::pdz_mouse;
  if (!cvars::pdz_mousehook || !window->HasFocus() || !captured.load()) {
    Reset(); return;
  }
  auto* kernel = kernel::kernel_state();
  if (!kernel || kernel->title_id() != 0x4D5307D3) { Reset(); return; }
  static winkey::PerfectDarkZeroGame game;
  if (!game.IsGameSupported(0)) { Reset(); return; }
  winkey::RawInputState input{};
  input.mouse.x_delta = dx.exchange(0);
  input.mouse.y_delta = dy.exchange(0);
  if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) state->gamepad.right_trigger = 255;
  if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) state->gamepad.left_trigger = 255;
  if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) state->gamepad.buttons |= X_INPUT_GAMEPAD_RIGHT_SHOULDER;
  game.DoHooks(user_index, input, state);
  if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) game.ModifierKeyHandler(user_index, input, state);
}
}
''')
cmake = repo / 'src/xenia/hid/keyboard/CMakeLists.txt'
with cmake.open('a') as f:
    f.write('''\nif(WIN32)
  target_sources(xenia-hid-keyboard PRIVATE ../winkey/hookables/PerfectDarkZero.cc)
endif()
''')
print('PDZ hook and Windows raw mouse adapter ported.')
