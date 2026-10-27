#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>
namespace pdz_menu {
// Guest thread owns native XUI. The host renderer only sends requests.
enum Page : unsigned { Closed, Options, Updates, Changelog, Extended, Results, Progression, Cheats };
inline std::atomic<unsigned> page{Closed};
inline std::atomic<bool> back{false};
inline std::atomic<int> challenge_request{-1};
inline std::mutex input_mutex;
struct Key { uint16_t code, flags; };
inline std::vector<Key> keys;
inline bool Overlay() { return page.load() >= Updates; }
inline bool KeyEvent(uint16_t code, uint16_t flags) {
  if (!Overlay()) return false;
  std::lock_guard lock(input_mutex);
  if (keys.size() < 128) keys.push_back({code,flags});
  return true;
}
}

