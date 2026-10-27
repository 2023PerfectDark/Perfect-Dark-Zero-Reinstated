/**
 * Unit tests for deleting host files on the last handle's close
 * (xenia-edge d8e5dae74).
 *
 * Ported for the ReXGlue fork.
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

TEST_CASE("Host deletion waits for the last open handle", "[filesystem][vfs]") {
  struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("rexglue_vfs_delete_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } temporary;
  REQUIRE(std::filesystem::create_directories(temporary.path / "dir"));
  std::ofstream(temporary.path / "shared.tmp") << "abc";
  std::ofstream(temporary.path / "dir" / "inner.tmp") << "abc";

  HostPathDevice device("\\Device\\Test", temporary.path, false);
  REQUIRE(device.Initialize());
  auto close = [](File* file) {
    if (file) {
      file->Destroy();
    }
  };
  auto open = [&](Entry* entry) {
    File* file = nullptr;
    REQUIRE(entry->Open(FileAccess::kGenericRead, &file) == X_STATUS_SUCCESS);
    return std::unique_ptr<File, decltype(close)>(file, close);
  };

  SECTION("A file is deleted once its last handle closes") {
    Entry* entry = device.ResolvePath("shared.tmp");
    REQUIRE(entry);
    auto first = open(entry);
    auto second = open(entry);
    entry->SetForDeletion(true);
    REQUIRE_FALSE(entry->Delete());
    first.reset();
    REQUIRE(std::filesystem::exists(temporary.path / "shared.tmp"));
    std::array<uint8_t, 3> bytes{};
    size_t read = 0;
    REQUIRE(second->ReadSync(bytes, 0, &read) == X_STATUS_SUCCESS);
    REQUIRE(read == bytes.size());
    REQUIRE(bytes == std::array<uint8_t, 3>{'a', 'b', 'c'});
    second.reset();
    REQUIRE_FALSE(std::filesystem::exists(temporary.path / "shared.tmp"));
    REQUIRE(device.ResolvePath("shared.tmp") == nullptr);
  }

  SECTION("A directory is kept while a file inside it is open") {
    Entry* directory = device.ResolvePath("dir");
    REQUIRE(directory);
    Entry* inner = directory->GetChild("inner.tmp");
    REQUIRE(inner);
    auto file = open(inner);
    REQUIRE_FALSE(directory->Delete());
    REQUIRE(std::filesystem::exists(temporary.path / "dir" / "inner.tmp"));
    file.reset();
    REQUIRE(directory->Delete());
    REQUIRE_FALSE(std::filesystem::exists(temporary.path / "dir"));
    REQUIRE(device.ResolvePath("dir") == nullptr);
  }
}
