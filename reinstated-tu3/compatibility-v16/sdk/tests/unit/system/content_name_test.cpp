/**
 * @file        system/content_name_test.cpp
 * @brief       Tests for the FATX check of content package file names
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <filesystem>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include <rex/system/xam/content_manager.h>

using rex::X_RESULT;
using rex::system::xam::ContentManager;
using rex::system::xam::XCONTENT_AGGREGATE_DATA;

namespace {

X_RESULT Check(const ContentManager& manager, const std::string_view name) {
  XCONTENT_AGGREGATE_DATA data{};
  data.set_file_name(name);
  return manager.CheckPackageName(data);
}

}  // namespace

TEST_CASE("Content file names FATX accepts name a package", "[system][content]") {
  const ContentManager manager(nullptr, std::filesystem::path("content"));
  CHECK(Check(manager, "SaveGame") == X_ERROR_SUCCESS);
  CHECK(Check(manager, "profile.dat") == X_ERROR_SUCCESS);
  CHECK(Check(manager, "a b-c_d~e") == X_ERROR_SUCCESS);
}

TEST_CASE("Content file names FATX refuses are invalid", "[system][content]") {
  const ContentManager manager(nullptr, std::filesystem::path("content"));
  CHECK(Check(manager, ".") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "..") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "..\\..\\escape") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "../escape") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "C:save") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "save?") == X_ERROR_INVALID_NAME);
  CHECK(Check(manager, "tab\tname") == X_ERROR_INVALID_NAME);
}

TEST_CASE("Content file names that name no package are not found", "[system][content]") {
  const ContentManager manager(nullptr, std::filesystem::path("content"));
  CHECK(Check(manager, "") == X_ERROR_PATH_NOT_FOUND);
  CHECK(Check(manager, "dir\\save") == X_ERROR_PATH_NOT_FOUND);
  CHECK(Check(manager, "caf\xC3\xA9") == X_ERROR_PATH_NOT_FOUND);
  CHECK(Check(manager, "bad\xFF") == X_ERROR_PATH_NOT_FOUND);
}

TEST_CASE("ContentExists is false for a name that leaves the package root", "[system][content]") {
  ContentManager manager(nullptr, std::filesystem::path("content"));
  XCONTENT_AGGREGATE_DATA data{};
  data.set_file_name("..\\..\\..");
  CHECK_FALSE(manager.ContentExists(0, data));
}

TEST_CASE("Root names must be plain ASCII names", "[system][content]") {
  CHECK(ContentManager::CheckRootName("save") == X_ERROR_SUCCESS);
  CHECK(ContentManager::CheckRootName("SAVE_1") == X_ERROR_SUCCESS);
  CHECK(ContentManager::CheckRootName("") == X_ERROR_INVALID_NAME);
  CHECK(ContentManager::CheckRootName("sa:ve") == X_ERROR_INVALID_NAME);
  CHECK(ContentManager::CheckRootName("a\\b") == X_ERROR_INVALID_NAME);
  // Not UTF-8: the string key helpers would throw on it.
  CHECK(ContentManager::CheckRootName("bad\xFF") == X_ERROR_INVALID_NAME);
  CHECK(ContentManager::CheckRootName("caf\xC3\xA9") == X_ERROR_INVALID_NAME);
}

TEST_CASE("Closing a root name that is not UTF-8 does not throw", "[system][content]") {
  ContentManager manager(nullptr, std::filesystem::path("content"));
  X_RESULT result = X_ERROR_SUCCESS;
  REQUIRE_NOTHROW(result = manager.CloseContent("bad\xFF"));
  CHECK(result == X_ERROR_FILE_NOT_FOUND);
}
