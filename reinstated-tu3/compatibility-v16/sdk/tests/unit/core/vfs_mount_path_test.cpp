/**
 * Unit tests for how VirtualFileSystem::ResolvePath picks a device by its mount
 * path (xenia-edge 9daef643e, 037eef540, d15858087).
 *
 * Ported for the ReXGlue fork.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/vfs.h>

using rex::X_STATUS;
using rex::filesystem::Device;
using rex::filesystem::Entry;
using rex::filesystem::FileAccess;
using rex::filesystem::FileAction;
using rex::filesystem::FileDisposition;
using rex::filesystem::HostPathDevice;
using rex::filesystem::VirtualFileSystem;

namespace {

struct TemporaryDirectory {
  explicit TemporaryDirectory(const char* file) {
    path = std::filesystem::temp_directory_path() /
           ("rexglue_vfs_mount_" + std::to_string(counter_++) + "_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
    std::ofstream(path / file) << "data";
  }
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  std::filesystem::path path;
  static inline int counter_ = 0;
};

Device* Mount(VirtualFileSystem& vfs, std::string_view mount_path,
              const std::filesystem::path& host_path) {
  auto device = std::make_unique<HostPathDevice>(mount_path, host_path, true);
  REQUIRE(device->Initialize());
  Device* raw = device.get();
  REQUIRE(vfs.RegisterDevice(std::move(device)));
  return raw;
}

}  // namespace

TEST_CASE("A mount path matches only a whole path component", "[filesystem][vfs]") {
  TemporaryDirectory content("a.bin");
  TemporaryDirectory content_eng("b.bin");
  VirtualFileSystem vfs;
  Mount(vfs, "\\Device\\Content", content.path);
  Device* eng = Mount(vfs, "\\Device\\Content_Eng", content_eng.path);

  Entry* entry = vfs.ResolvePath("\\Device\\Content_Eng\\b.bin");
  REQUIRE(entry != nullptr);
  CHECK(entry->device() == eng);
  CHECK(vfs.ResolvePath("\\Device\\Content\\a.bin") != nullptr);
  CHECK(vfs.ResolvePath("\\Device\\ContentX\\a.bin") == nullptr);
}

TEST_CASE("The longest matching mount path wins", "[filesystem][vfs]") {
  TemporaryDirectory outer("outer.bin");
  TemporaryDirectory inner("inner.bin");
  VirtualFileSystem vfs;
  Mount(vfs, "\\Device\\Harddisk0", outer.path);
  Device* nested = Mount(vfs, "\\Device\\Harddisk0\\Partition1", inner.path);

  Entry* entry = vfs.ResolvePath("\\Device\\Harddisk0\\Partition1\\inner.bin");
  REQUIRE(entry != nullptr);
  CHECK(entry->device() == nested);
  CHECK(vfs.ResolvePath("\\Device\\Harddisk0\\outer.bin") != nullptr);
}

TEST_CASE("A link to a package root without its trailing separator resolves",
          "[filesystem][vfs]") {
  TemporaryDirectory package("a.bin");
  VirtualFileSystem vfs;
  Device* device = Mount(vfs, "\\Device\\Content\\1\\", package.path);
  // ObCreateSymbolicLink canonicalizes the target to drop the separator.
  REQUIRE(vfs.RegisterSymbolicLink("pkg:", "\\Device\\Content\\1"));

  Entry* root = vfs.ResolvePath("pkg:");
  REQUIRE(root != nullptr);
  CHECK(root->device() == device);

  rex::filesystem::File* file = nullptr;
  FileAction action = FileAction::kDoesNotExist;
  REQUIRE(vfs.OpenFile(nullptr, "pkg:\\a.bin", FileDisposition::kOpen, FileAccess::kGenericRead,
                       false, true, &file, &action) == X_STATUS_SUCCESS);
  REQUIRE(file != nullptr);
  file->Destroy();
}
