#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <array>

namespace pdz_input {
// Separate press/release intervals keep repeated wheel detents visible even
// when the game polls input several times per rendered frame.
class ButtonPulses {
 public:
  void Push(uint16_t button) { if (queue_.size() < 16) queue_.push_back(button); }
  void Clear() { queue_.clear(); button_ = 0; until_ = 0; releasing_ = false; }
  uint16_t Poll(int64_t now) {
    if (now < until_) return releasing_ ? 0 : button_;
    if (button_ && !releasing_) {
      releasing_ = true; until_ = now + 50; return 0;
    }
    button_ = 0; releasing_ = false;
    if (!queue_.empty()) {
      button_ = queue_.front(); queue_.pop_front(); until_ = now + 80;
    }
    return button_;
  }
 private:
  std::deque<uint16_t> queue_;
  uint16_t button_ = 0;
  int64_t until_ = 0;
  bool releasing_ = false;
};
struct InventoryLayout {
  std::array<int, 14> starts{};
  int pages = 1;
};
inline InventoryLayout PaginateInventory(const std::array<int, 13>& sizes, int count) {
  InventoryLayout layout;
  count = std::clamp(count, 0, 13);
  int used = 0;
  for (int i = 0; i < count; ++i) {
    const int size = std::clamp(sizes[i], 1, 3);
    if (used + size > 4) { layout.starts[layout.pages++] = i; used = 0; }
    used += size;
  }
  layout.starts[layout.pages] = count;
  return layout;
}
inline int AimPressure(int pressure, int detents) {
  return std::clamp(pressure + std::clamp(detents, -16, 16) * 16, 32, 255);
}
inline uint8_t MergeMouseAimTrigger(uint8_t native, bool held, bool dual, int pressure) {
  if (!held) return native;
  // In dual wield the left trigger fires a gun, rather than adjusting aim.
  return std::max(native, uint8_t(dual ? 255 : std::clamp(pressure, 32, 255)));
}
}
