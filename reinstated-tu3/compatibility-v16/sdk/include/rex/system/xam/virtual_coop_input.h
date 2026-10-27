#pragma once
#include <cstdint>
#include <mutex>

namespace rex::system::xam {
struct VirtualCoopCommand {
  uint16_t buttons{};
  uint8_t left_trigger{}, right_trigger{};
  int16_t left_x{}, left_y{}, right_x{}, right_y{};
  bool operator==(const VirtualCoopCommand&) const = default;
};
struct VirtualCoopSnapshot {
  uint32_t packet{};
  VirtualCoopCommand command{};
};

// A command is a short lease, not a latched controller state. Pauses, loading,
// or a failed AI update must release firing and movement automatically.
class VirtualCoopInput {
 public:
  void Submit(VirtualCoopCommand command, uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    Set(command);
    updated_ms_ = now_ms;
    valid_ = true;
  }
  VirtualCoopSnapshot Read(uint64_t now_ms) {
    std::lock_guard lock(mutex_);
    if (!valid_ || now_ms < updated_ms_ || now_ms - updated_ms_ >= 250) {
      Set({});
      valid_ = false;
    }
    return state_;
  }
  void Reset() {
    std::lock_guard lock(mutex_);
    Set({});
    valid_ = false;
  }
 private:
  void Set(VirtualCoopCommand command) {
    if (!(state_.command == command)) {
      state_.command = command;
      ++state_.packet;
    }
  }
  std::mutex mutex_;
  VirtualCoopSnapshot state_{};
  uint64_t updated_ms_{};
  bool valid_{};
};
void SubmitVirtualCoopInput(VirtualCoopCommand command);
void ResetVirtualCoopInput();
}
