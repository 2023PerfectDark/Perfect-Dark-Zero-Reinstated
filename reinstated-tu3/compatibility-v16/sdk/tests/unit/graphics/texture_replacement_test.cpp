/**
 * @file        graphics/texture_replacement_test.cpp
 * @brief       Tests for texture replacement packs: keys, index lines, DDS
 *              headers, alpha checks and texture dumping
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <rex/graphics/pipeline/texture/address.h>
#include <rex/graphics/pipeline/texture/replacement.h>
#include <rex/graphics/xenos.h>
#include <rex/hash.h>

using namespace rex::graphics;
namespace tr = rex::graphics::texture_replacement;

namespace {

// The tiled offset of the cod3 series (rexglue PR #524, util.cpp
// GetTiledOffset2D), which its pack ripper computes keys with.
int32_t RipperTiledOffset2D(int32_t x, int32_t y, uint32_t pitch, uint32_t bytes_per_block_log2) {
  pitch = (pitch + 31) & ~uint32_t(31);
  int32_t macro = ((x >> 5) + (y >> 5) * int32_t(pitch >> 5)) << (bytes_per_block_log2 + 7);
  int32_t micro = ((x & 7) + ((y & 0xE) << 2)) << bytes_per_block_log2;
  int32_t offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F);
}

}  // namespace

// Keys must match the ones pack tools compute, so the tiled addressing the key
// is gathered with must be the ripper's.
TEST_CASE("Texture replacement tiling matches the pack ripper", "[graphics][texture]") {
  for (uint32_t bpb_log2 : {2u, 3u, 4u}) {
    for (uint32_t pitch : {32u, 64u, 96u}) {
      for (int32_t y = 0; y < 72; ++y) {
        for (int32_t x = 0; x < int32_t(pitch); ++x) {
          REQUIRE(texture_address::Tiled2D(x, y, pitch, bpb_log2) ==
                  RipperTiledOffset2D(x, y, pitch, bpb_log2));
        }
      }
    }
  }
}

TEST_CASE("Texture replacement key of a linear level", "[graphics][texture]") {
  // 2x2 k_8_8_8_8, pitch 32 texels, 8-in-32 swap: rows of 2 texels at 128-byte
  // strides, each texel reversed.
  std::vector<uint8_t> guest(32 * 4 * 2, 0xEE);
  const uint8_t texels[4][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
  std::memcpy(guest.data(), texels[0], 8);
  std::memcpy(guest.data() + 128, texels[2], 8);
  tr::GuestBaseLevel level;
  level.data = guest.data();
  level.data_size = uint32_t(guest.size());
  level.width = 2;
  level.height = 2;
  level.pitch_texels = 32;
  level.format = xenos::TextureFormat::k_8_8_8_8;
  level.endianness = xenos::Endian::k8in32;
  std::vector<uint8_t> scratch;
  uint64_t key = 0;
  REQUIRE(tr::ComputeKey(level, scratch, key));
  const uint8_t expected[16] = {4, 3, 2, 1, 8, 7, 6, 5, 12, 11, 10, 9, 16, 15, 14, 13};
  REQUIRE(scratch.size() == sizeof(expected));
  CHECK(std::memcmp(scratch.data(), expected, sizeof(expected)) == 0);
  CHECK(key == XXH64(expected, sizeof(expected), 0));

  // A level that doesn't fit in the data has no key.
  level.data_size = 128 + 4;
  CHECK_FALSE(tr::ComputeKey(level, scratch, key));
}

TEST_CASE("Texture replacement index fields", "[graphics][texture]") {
  uint64_t key = 0;
  CHECK(tr::ParseKey("00000000DeadBeef", key));
  CHECK(key == 0xDEADBEEF);
  CHECK_FALSE(tr::ParseKey("", key));
  CHECK_FALSE(tr::ParseKey("12345678901234567", key));
  CHECK_FALSE(tr::ParseKey("12g4", key));

  CHECK(tr::IsPlainPackFile("textures/wall.dds"));
  CHECK(tr::IsPlainPackFile("a.dds"));
  CHECK_FALSE(tr::IsPlainPackFile("/etc/passwd"));
  CHECK_FALSE(tr::IsPlainPackFile("../outside.dds"));
  CHECK_FALSE(tr::IsPlainPackFile("a/./b.dds"));
  CHECK_FALSE(tr::IsPlainPackFile("a//b.dds"));
  CHECK_FALSE(tr::IsPlainPackFile("dir/"));
  CHECK_FALSE(tr::IsPlainPackFile("C:/x.dds"));
  CHECK_FALSE(tr::IsPlainPackFile("a\\b.dds"));
  CHECK_FALSE(tr::IsPlainPackFile(std::string(256, 'a')));
}

TEST_CASE("Texture replacement image size and levels", "[graphics][texture]") {
  CHECK(tr::IsReplacementSize(256, 128, 512, 256));
  CHECK(tr::IsReplacementSize(256, 128, 640, 320));
  // 3x of 100x60 rounded to multiples of 4.
  CHECK(tr::IsReplacementSize(100, 60, 300, 180));
  CHECK(tr::IsReplacementSize(100, 60, 300, 184));
  CHECK_FALSE(tr::IsReplacementSize(256, 128, 512, 512));
  CHECK_FALSE(tr::IsReplacementSize(0, 128, 512, 256));

  // The guest's levels plus round(log2(scale)), at most a full chain.
  CHECK(tr::GetReplacementLevelCount(256, 256, 9, 1024, 1024) == 11);
  CHECK(tr::GetReplacementLevelCount(256, 256, 9, 256, 256) == 9);
  CHECK(tr::GetReplacementLevelCount(256, 256, 1, 768, 768) == 3);
  CHECK(tr::GetReplacementLevelCount(4, 4, 3, 16, 16) == 5);
  CHECK(tr::GetReplacementLevelCount(256, 256, 9, 128, 128) == 8);
}

TEST_CASE("Texture replacement DDS headers round-trip", "[graphics][texture]") {
  const xenos::TextureFormat formats[] = {
      xenos::TextureFormat::k_DXT1, xenos::TextureFormat::k_DXT2_3,
      xenos::TextureFormat::k_DXT4_5, xenos::TextureFormat::k_DXN,
      xenos::TextureFormat::k_8_8_8_8};
  for (xenos::TextureFormat format : formats) {
    std::vector<uint8_t> header = tr::MakeDdsHeader(format, 64, 32);
    tr::DdsInfo info;
    std::string error;
    REQUIRE(tr::ParseDdsHeader(header.data(), header.size(), info, error));
    CHECK(info.format == format);
    CHECK(info.width == 64);
    CHECK(info.height == 32);
    CHECK(info.level_count == 1);
    const uint32_t block_size = tr::GetBlockSize(format);
    CHECK(info.data_size ==
          uint64_t(64 / block_size) * (32 / block_size) * tr::GetBytesPerBlock(format));
    CHECK(info.has_alpha);
    for (int i = 0; i < 4; ++i) {
      CHECK(info.bgra_source_bytes[i] == i);
    }
  }

  std::vector<uint8_t> header = tr::MakeDdsHeader(xenos::TextureFormat::k_DXT1, 64, 64);
  tr::DdsInfo info;
  std::string error;
  // A DX10 header.
  std::vector<uint8_t> dx10 = header;
  std::memcpy(dx10.data() + 4 + 72 + 8, "DX10", 4);
  CHECK_FALSE(tr::ParseDdsHeader(dx10.data(), dx10.size(), info, error));
  // A cube map.
  std::vector<uint8_t> cube = header;
  cube[4 + 108 + 1] = 0x02;
  CHECK_FALSE(tr::ParseDdsHeader(cube.data(), cube.size(), info, error));
  // More levels than the image can have.
  std::vector<uint8_t> levels = header;
  levels[4 + 4 + 2] |= 0x02;  // DDSD_MIPMAPCOUNT
  levels[4 + 24] = 8;
  CHECK_FALSE(tr::ParseDdsHeader(levels.data(), levels.size(), info, error));
  CHECK_FALSE(tr::ParseDdsHeader(header.data(), 64, info, error));
}

TEST_CASE("Texture replacement alpha checks", "[graphics][texture]") {
  // A k_DXT1 block in the three-color mode (color0 <= color1) with every
  // index 3: transparent.
  const uint8_t transparent_block[8] = {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  tr::AlphaStats stats;
  REQUIRE(tr::GetAlphaStats(xenos::TextureFormat::k_DXT1, 4, 4, transparent_block, 8, stats));
  CHECK_FALSE(stats.opaque);
  CHECK(stats.texels == 16);
  CHECK(stats.visible == 0);

  tr::Image image;
  image.format = xenos::TextureFormat::k_DXT1;
  image.block_size = 4;
  image.width = image.height = 4;
  image.levels.push_back({4, 4, 8, 1, 0});
  image.data.assign(transparent_block, transparent_block + 8);
  CHECK(tr::ForceOpaque(image) == 1);
  REQUIRE(tr::GetAlphaStats(xenos::TextureFormat::k_DXT1, 4, 4, image.data.data(), 8, stats));
  CHECK(stats.opaque);

  // k_8_8_8_8: an original half see-through refuses a fully see-through image,
  // and makes an image for an opaque original opaque.
  std::vector<uint8_t> guest(4 * 4, 0xFF);
  guest[3] = guest[7] = 0;
  tr::Image rgba;
  rgba.format = xenos::TextureFormat::k_8_8_8_8;
  rgba.width = rgba.height = 2;
  rgba.levels.push_back({2, 2, 8, 2, 0});
  rgba.data.assign(16, 0);
  uint64_t forced = 0;
  std::string error;
  CHECK_FALSE(tr::EnforceAlpha(2, 2, guest.data(), guest.size(), rgba, forced, error));
  guest[3] = guest[7] = 0xFF;
  CHECK(tr::EnforceAlpha(2, 2, guest.data(), guest.size(), rgba, forced, error));
  CHECK(forced == 4);
  CHECK(rgba.data[3] == 0xFF);
}

TEST_CASE("Texture replacement dump is a pack", "[graphics][texture]") {
  const std::filesystem::path folder =
      std::filesystem::temp_directory_path() / "rexglue_texture_replacement_test";
  std::filesystem::remove_all(folder);
  std::filesystem::create_directories(folder);

  // An 8x4 k_DXT1 base level: two blocks.
  std::vector<uint8_t> level(16);
  for (size_t i = 0; i < level.size(); ++i) {
    level[i] = uint8_t(i * 7 + 1);
  }
  const uint64_t key = XXH64(level.data(), level.size(), 0);
  std::string error;
  REQUIRE(tr::DumpTexture(folder, key, xenos::TextureFormat::k_DXT1, 8, 4, level.data(),
                          level.size(), error));
  // Dumped once.
  REQUIRE(tr::DumpTexture(folder, key, xenos::TextureFormat::k_DXT1, 8, 4, level.data(),
                          level.size(), error));
  CHECK_FALSE(tr::DumpTexture(folder, key + 1, xenos::TextureFormat::k_DXT1, 8, 4, level.data(),
                              level.size() - 1, error));

  tr::Index index;
  REQUIRE(index.Load(folder, false, error));
  CHECK(index.size() == 1);
  CHECK(index.skipped_lines() == 0);
  const tr::IndexEntry* entry = index.Find(key);
  REQUIRE(entry != nullptr);
  CHECK(entry->width == 8);
  CHECK(entry->height == 4);
  CHECK(entry->format == xenos::TextureFormat::k_DXT1);
  CHECK(tr::IsInsideFolder(folder, index.FilePath(*entry)));

  tr::Image image;
  REQUIRE(tr::LoadDds(index.FilePath(*entry), xenos::TextureFormat::k_DXT1, 8, 4, 1, 16384, 0,
                      image, error));
  CHECK(image.width == 8);
  CHECK(image.height == 4);
  REQUIRE(image.levels.size() == 1);
  CHECK(image.data == level);

  // The wrong format, or a size of another aspect ratio, isn't used.
  CHECK_FALSE(tr::LoadDds(index.FilePath(*entry), xenos::TextureFormat::k_DXT4_5, 8, 4, 1, 16384,
                          0, image, error));
  CHECK_FALSE(tr::LoadDds(index.FilePath(*entry), xenos::TextureFormat::k_DXT1, 256, 1024, 1,
                          16384, 0, image, error));
  // Over the memory limit.
  bool too_large = false;
  CHECK_FALSE(tr::LoadDds(index.FilePath(*entry), xenos::TextureFormat::k_DXT1, 8, 4, 1, 16384, 8,
                          image, error, &too_large));
  CHECK(too_large);

  std::filesystem::remove_all(folder);
}
