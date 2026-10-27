#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>

// Coverage index only. The SDK shader store owns the actual microcode and
// pipeline descriptions and precompiles that accumulated store on startup.
namespace pdz_shader_profiles {
inline std::atomic<int> mission{-1}, mode{-1};
inline std::mutex mutex;
struct Profile {
  std::set<uint64_t> hashes;
  std::ofstream stream;
};
inline std::filesystem::path root;
inline std::map<std::string, Profile> profiles;
inline auto last_flush = std::chrono::steady_clock::now();
inline void SetMap(int new_mode, int id, const char* label) {
  std::lock_guard lock(mutex);
  for (auto& [name, profile] : profiles) profile.stream.flush();
  if (!root.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (!ec) {
      std::string safe;
      for (const char* p = label; p && *p; ++p)
        safe += (*p == '"' || *p == '\\' || static_cast<unsigned char>(*p) < 32) ? '_' : *p;
      std::ofstream journal(root / "map-loads.jsonl", std::ios::app);
      const auto now = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      journal << "{\"unix_time\":" << now << ",\"event\":\""
              << (id < 0 ? "map_end" : "map_load") << "\",\"mode\":"
              << (id < 0 ? mode.load() : new_mode)
              << ",\"map_id\":" << (id < 0 ? mission.load() : id)
              << ",\"asset\":\"" << safe << "\"}\n";
    }
  }
  mode = new_mode;
  mission = id;
}
inline void Initialize(const std::filesystem::path& cache) {
  std::lock_guard lock(mutex);
  root = cache / "pdz-mission-shaders";
}
inline void Flush() {
  std::lock_guard lock(mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now - last_flush < std::chrono::seconds(5)) return;
  for (auto& [name, profile] : profiles) profile.stream.flush();
  last_flush = now;
}
inline void Record(const char* backend, uint64_t hash) {
  if (mission.load() < 0) return;
  std::lock_guard lock(mutex);
  const int id = mission.load();
  if (id < 0 || id > 255) return;
  if (root.empty()) return;
  const auto name = "mode-" + std::to_string(mode.load()) + "-map-" +
                    std::to_string(id) + "-" + backend + ".pipelines";
  auto [it, inserted] = profiles.try_emplace(name);
  auto& profile = it->second;
  if (inserted) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return;
    std::ifstream previous(root / name);
    uint64_t old;
    while (previous >> std::hex >> old) profile.hashes.insert(old);
    profile.stream.open(root / name, std::ios::app);
  }
  if (profile.hashes.insert(hash).second)
    profile.stream << std::hex << hash << '\n';
}
}
