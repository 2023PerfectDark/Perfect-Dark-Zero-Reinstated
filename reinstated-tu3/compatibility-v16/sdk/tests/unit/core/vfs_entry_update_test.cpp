/**
 * Unit tests for HostPathEntry::update refreshing the entry timestamps
 * (xenia-edge 211e7d953).
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

TEST_CASE("A host entry update picks up the new write time", "[filesystem][vfs]") {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("rexglue_vfs_update_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  const std::filesystem::path file = dir / "save.bin";
  std::ofstream(file) << "data";

  {
    HostPathDevice device("\\Device\\Test", dir, false);
    REQUIRE(device.Initialize());
    Entry* entry = device.ResolvePath("save.bin");
    REQUIRE(entry != nullptr);
    const uint64_t first = entry->write_timestamp();

    const auto earlier = std::filesystem::last_write_time(file) - std::chrono::hours(24);
    std::filesystem::last_write_time(file, earlier);
    entry->update();
    CHECK(entry->write_timestamp() < first);
  }

  std::error_code error;
  std::filesystem::remove_all(dir, error);
}
