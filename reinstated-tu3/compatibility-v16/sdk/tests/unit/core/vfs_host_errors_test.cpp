/**
 * Unit tests for host file system refusals failing the guest's call instead of
 * throwing (ForzaHorizon1-ReImagined SDK patch 0097).
 *
 * Ported for the ReXGlue fork.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>

using rex::filesystem::Entry;
using rex::filesystem::HostPathDevice;
using rex::filesystem::kFileAttributeDirectory;

TEST_CASE("A directory the host refuses to create fails without throwing", "[filesystem][vfs]") {
  struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("rexglue_vfs_host_errors_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } temporary;
  REQUIRE(std::filesystem::create_directories(temporary.path / "dir"));

  HostPathDevice device("\\Device\\Test", temporary.path, false);
  REQUIRE(device.Initialize());
  Entry* directory = device.ResolvePath("dir");
  REQUIRE(directory);

  // A host file the VFS has not seen yet holds the name: creating a directory
  // there is refused by the host.
  std::ofstream(temporary.path / "dir" / "blocker") << "abc";
  Entry* created = nullptr;
  REQUIRE_NOTHROW(created = directory->CreateEntry("blocker", kFileAttributeDirectory));
  CHECK(created == nullptr);
  CHECK(std::filesystem::is_regular_file(temporary.path / "dir" / "blocker"));
}
