/**
 * Unit tests for Entry::Rename on a host path device (xenia-edge b5baa311a).
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

using rex::X_STATUS;
using rex::filesystem::Entry;
using rex::filesystem::HostPathDevice;

namespace {

struct TemporaryDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("rexglue_vfs_rename_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

}  // namespace

TEST_CASE("Host path entry moves into the directory it is renamed into", "[filesystem][vfs]") {
  TemporaryDirectory temporary;
  const std::filesystem::path& dir = temporary.path;
  REQUIRE(std::filesystem::create_directories(dir / "pack"));
  std::ofstream(dir / "temp.bin") << "data";

  HostPathDevice device("\\Device\\Test", dir, false);
  REQUIRE(device.Initialize());
  Entry* entry = device.ResolvePath("temp.bin");
  REQUIRE(entry != nullptr);

  SECTION("into an existing directory") {
    REQUIRE(entry->Rename("cache:\\pack\\item.arc") == X_STATUS_SUCCESS);

    CHECK(device.ResolvePath("pack\\item.arc") == entry);
    CHECK(entry->parent() == device.ResolvePath("pack"));
    CHECK(entry->path() == "pack\\item.arc");
    CHECK(device.ResolvePath("item.arc") == nullptr);
    CHECK(device.ResolvePath("temp.bin") == nullptr);
    CHECK(std::filesystem::exists(dir / "pack" / "item.arc"));
  }

  SECTION("into a missing directory") {
    CHECK(entry->Rename("cache:\\missing\\item.arc") == X_STATUS_OBJECT_PATH_NOT_FOUND);

    CHECK(device.ResolvePath("temp.bin") == entry);
    CHECK(entry->name() == "temp.bin");
    CHECK(std::filesystem::exists(dir / "temp.bin"));
  }
}

TEST_CASE("Host path rename checks the target name", "[filesystem][vfs]") {
  TemporaryDirectory temporary;
  REQUIRE(std::filesystem::create_directories(temporary.path / "dir"));
  std::ofstream(temporary.path / "new.bin") << "new";
  std::ofstream(temporary.path / "old.bin") << "old";

  HostPathDevice device("test:", temporary.path, false);
  REQUIRE(device.Initialize());
  Entry* entry = device.ResolvePath("new.bin");
  Entry* old_entry = device.ResolvePath("old.bin");
  REQUIRE(entry);
  REQUIRE(old_entry);
  auto read = [&](const char* name) {
    std::string text;
    std::ifstream(temporary.path / name) >> text;
    return text;
  };

  SECTION("An existing file is kept without replace") {
    CHECK(entry->Rename("cache:\\OLD.BIN") == X_STATUS_OBJECT_NAME_COLLISION);
    CHECK(device.ResolvePath("old.bin") == old_entry);
    CHECK(device.ResolvePath("new.bin") == entry);
    CHECK(read("old.bin") == "old");
  }

  SECTION("An existing file is replaced with replace") {
    CHECK(entry->Rename("cache:\\old.bin", true) == X_STATUS_SUCCESS);
    CHECK(device.ResolvePath("old.bin") == entry);
    CHECK(device.ResolvePath("new.bin") == nullptr);
    CHECK(device.ResolvePath("")->child_count() == 2);
    CHECK(read("old.bin") == "new");
  }

  SECTION("A directory is not replaced") {
    CHECK(entry->Rename("cache:\\dir", true) == X_STATUS_OBJECT_NAME_COLLISION);
    CHECK(std::filesystem::is_directory(temporary.path / "dir"));
  }

  SECTION("Its own name is refused") {
    CHECK(entry->Rename("cache:\\NEW.BIN") == X_STATUS_OBJECT_NAME_COLLISION);
    CHECK(entry->Rename("cache:\\NEW.BIN", true) == X_STATUS_ACCESS_DENIED);
    CHECK(entry->name() == "new.bin");
  }
}
