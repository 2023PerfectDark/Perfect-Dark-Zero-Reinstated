/**
 * @file        core/vfs_overlay_test.cpp
 * @brief       Tests for host path device overlays (content_overlay_root)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ArmyOfTwoRecompilation
 *              (sdk_changes, host_path_device.cpp).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>

using rex::X_STATUS;
using rex::filesystem::Entry;
using rex::filesystem::File;
using rex::filesystem::FileAccess;
using rex::filesystem::HostPathDevice;

namespace {

std::string ReadAll(Entry* entry) {
  File* file = nullptr;
  REQUIRE(entry->Open(FileAccess::kGenericRead, &file) == X_STATUS_SUCCESS);
  std::array<uint8_t, 64> bytes{};
  size_t read = 0;
  REQUIRE(file->ReadSync(bytes, 0, &read) == X_STATUS_SUCCESS);
  file->Destroy();
  return std::string(bytes.begin(), bytes.begin() + read);
}

}  // namespace

TEST_CASE("Overlay files stand in for the package's own", "[filesystem][vfs]") {
  struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("rexglue_vfs_overlay_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } temporary;
  const auto package = temporary.path / "package";
  const auto overlay = temporary.path / "overlay";
  REQUIRE(std::filesystem::create_directories(package / "sub"));
  REQUIRE(std::filesystem::create_directories(overlay / "sub"));
  std::ofstream(package / "text.int") << "original";
  std::ofstream(package / "sub" / "deep.int") << "original";
  std::ofstream(package / "kept.bin") << "kept";
  std::ofstream(overlay / "text.int") << "translated";
  std::ofstream(overlay / "sub" / "deep.int") << "translated";
  std::ofstream(overlay / "extra.int") << "extra";

  HostPathDevice device("\\Device\\Test", package, false);
  device.set_overlay_path(overlay);
  REQUIRE(device.Initialize());

  Entry* text = device.ResolvePath("text.int");
  REQUIRE(text);
  CHECK(ReadAll(text) == "translated");
  CHECK(text->size() == std::string("translated").size());
  Entry* deep = device.ResolvePath("sub\\deep.int");
  REQUIRE(deep);
  CHECK(ReadAll(deep) == "translated");
  // A file the overlay doesn't have is the package's own.
  Entry* kept = device.ResolvePath("kept.bin");
  REQUIRE(kept);
  CHECK(ReadAll(kept) == "kept");
  // An overlay file the package doesn't have is not added.
  CHECK(device.ResolvePath("extra.int") == nullptr);
}
