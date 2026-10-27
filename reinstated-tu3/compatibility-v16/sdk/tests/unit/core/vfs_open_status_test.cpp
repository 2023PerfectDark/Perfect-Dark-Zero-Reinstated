/**
 * Unit tests for the NT status VirtualFileSystem::OpenFile returns for a missing
 * file or parent directory (xenia-canary e91faca1c).
 *
 * Ported for the ReXGlue fork.
 */

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/vfs.h>

using rex::X_STATUS;
using rex::filesystem::FileAccess;
using rex::filesystem::FileAction;
using rex::filesystem::FileDisposition;
using rex::filesystem::HostPathDevice;
using rex::filesystem::VirtualFileSystem;

TEST_CASE("OpenFile reports a missing file and a missing directory apart", "[filesystem][vfs]") {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("rexglue_vfs_open_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);

  {
    VirtualFileSystem vfs;
    auto device = std::make_unique<HostPathDevice>("\\TestStore", dir, true);
    REQUIRE(device->Initialize());
    REQUIRE(vfs.RegisterDevice(std::move(device)));
    REQUIRE(vfs.RegisterSymbolicLink("test:", "\\TestStore"));

    rex::filesystem::File* file = nullptr;
    FileAction action = FileAction::kOpened;
    CHECK(vfs.OpenFile(nullptr, "test:\\missing.bin", FileDisposition::kOpen,
                       FileAccess::kGenericRead, false, true, &file,
                       &action) == X_STATUS_OBJECT_NAME_NOT_FOUND);
    CHECK(action == FileAction::kDoesNotExist);

    action = FileAction::kOpened;
    CHECK(vfs.OpenFile(nullptr, "test:\\missing\\file.bin", FileDisposition::kOpen,
                       FileAccess::kGenericRead, false, true, &file,
                       &action) == X_STATUS_OBJECT_PATH_NOT_FOUND);
    CHECK(action == FileAction::kDoesNotExist);
    CHECK(file == nullptr);
  }

  std::error_code error;
  std::filesystem::remove_all(dir, error);
}
