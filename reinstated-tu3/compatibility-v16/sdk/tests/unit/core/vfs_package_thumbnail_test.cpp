/**
 * Unit tests for hiding the runtime's package thumbnail from guest enumeration
 * (xenia-edge ed0e06112).
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

TEST_CASE("A package directory lists its save but not the thumbnail", "[filesystem][vfs]") {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("rexglue_vfs_thumb_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "save.dat") << "data";
  std::ofstream(dir / rex::filesystem::kPackageThumbnailFileName) << "png";

  {
    HostPathDevice device("\\Device\\Content\\1\\", dir, true, true, true);
    REQUIRE(device.Initialize());
    Entry* root = device.ResolvePath("");
    REQUIRE(root != nullptr);
    CHECK(root->child_count() == 1);
    CHECK(device.ResolvePath("save.dat") != nullptr);
  }

  std::error_code error;
  std::filesystem::remove_all(dir, error);
}
