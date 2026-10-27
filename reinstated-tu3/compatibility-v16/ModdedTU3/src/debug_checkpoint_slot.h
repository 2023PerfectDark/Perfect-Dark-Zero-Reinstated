#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace pdz_checkpoint {
// Storage only. A payload must come from a verified checkpoint serializer,
// never a dump of actor pointers or a position-only approximation.
struct Snapshot {
  uint32_t mission{}, difficulty{}, rules{};
  std::array<uint8_t, 32> build{};
  std::vector<uint8_t> payload;
};
inline uint32_t Checksum(std::span<const uint8_t> bytes) {
  uint32_t crc = ~0u;
  for (auto b : bytes) {
    crc ^= b;
    for (int n = 0; n < 8; ++n)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
class Slot {
 public:
  explicit Slot(const std::filesystem::path& profile)
      : file_(profile / "debug-checkpoint" / "single-mission.bin") {}
  bool Save(const Snapshot& snapshot) const {
    if (snapshot.payload.empty() || snapshot.payload.size() > kLimit) return false;
    std::vector<uint8_t> bytes{'P','D','Z','C','H','K','P','T'};
    Append(bytes, 1);
    Append(bytes, snapshot.mission);
    Append(bytes, snapshot.difficulty);
    Append(bytes, snapshot.rules);
    bytes.insert(bytes.end(), snapshot.build.begin(), snapshot.build.end());
    Append(bytes, static_cast<uint32_t>(snapshot.payload.size()));
    bytes.insert(bytes.end(), snapshot.payload.begin(), snapshot.payload.end());
    Append(bytes, Checksum(bytes));
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    if (ec) return false;
    auto temporary = file_; temporary += ".tmp";
    {
      std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      out.flush();
      if (!out) return false;
      out.close();
      if (!out) return false;
    }
    // Replace the one slot only after the complete new file has been written.
    // Never delete the old slot first: a failed replacement must preserve it.
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), file_.c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, file_, ec);
    return !ec;
#endif
  }
  bool Load(const std::array<uint8_t, 32>& build, Snapshot& result) const {
    std::error_code ec;
    const auto length = std::filesystem::file_size(file_, ec);
    if (ec || length < 65 || length > kLimit + 64) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    std::ifstream in(file_, std::ios::binary);
    if (!in.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    constexpr std::array<uint8_t, 8> magic{'P','D','Z','C','H','K','P','T'};
    if (!std::equal(magic.begin(), magic.end(), bytes.begin()) || Read(bytes, 8) != 1 ||
        Read(bytes, 56) != bytes.size() - 64 ||
        Read(bytes, bytes.size() - 4) != Checksum(std::span(bytes).first(bytes.size() - 4)) ||
        !std::equal(build.begin(), build.end(), bytes.begin() + 24)) return false;
    Snapshot next;
    next.mission = Read(bytes, 12); next.difficulty = Read(bytes, 16);
    next.rules = Read(bytes, 20); next.build = build;
    next.payload.assign(bytes.begin() + 60, bytes.end() - 4);
    result = std::move(next);
    return true;
  }
 private:
  static constexpr uint64_t kLimit = 512ull * 1024 * 1024;
  std::filesystem::path file_;
  static void Append(std::vector<uint8_t>& out, uint32_t value) {
    for (unsigned n = 0; n < 4; ++n) out.push_back(uint8_t(value >> (8 * n)));
  }
  static uint32_t Read(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    for (unsigned n = 0; n < 4; ++n) value |= uint32_t(bytes[offset + n]) << (8 * n);
    return value;
  }
};
}
