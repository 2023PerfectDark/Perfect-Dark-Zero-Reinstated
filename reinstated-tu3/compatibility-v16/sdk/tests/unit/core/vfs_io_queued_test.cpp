/**
 * Unit tests for which VFS devices queue every request, so that a request on an
 * async handle always returns X_STATUS_PENDING (xenia-canary 976c3578b).
 *
 * Ported for the ReXGlue fork.
 */

#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <rex/filesystem/devices/host_path_device.h>

using rex::filesystem::HostPathDevice;

TEST_CASE("A game folder reads as FATX and completes buffered I/O inline", "[filesystem][vfs]") {
  HostPathDevice device("\\Device\\Harddisk0\\Partition1", std::filesystem::temp_directory_path(),
                        true);
  CHECK(device.name() == "FATX");
  CHECK_FALSE(device.is_io_queued());
}

TEST_CASE("A content package folder reads as STFS and queues every request",
          "[filesystem][vfs]") {
  HostPathDevice device("\\Device\\Content\\1\\", std::filesystem::temp_directory_path(), true,
                        /*allow_share_delete=*/true, /*package=*/true);
  CHECK(device.name() == "STFS");
  CHECK(device.is_io_queued());
}
