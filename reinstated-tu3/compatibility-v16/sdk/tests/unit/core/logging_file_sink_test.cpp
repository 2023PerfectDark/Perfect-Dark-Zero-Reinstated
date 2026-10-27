/**
 * @file        core/logging_file_sink_test.cpp
 * @brief       Tests for the log file sink opened through std::filesystem::path
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <spdlog/logger.h>

#include <rex/logging.h>

namespace {

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("CreateFileSink writes to a non-ASCII path", "[logging]") {
  // Cyrillic and Japanese: on Windows at least one of them is outside any
  // ANSI code page, where path::string() would throw.
  const auto dir =
      std::filesystem::temp_directory_path() / std::filesystem::path(u8"rex_журнал_日本語");
  const auto log_path = dir / std::filesystem::path(u8"запуск.log");
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);

  {
    auto sink = rex::CreateFileSink(log_path);
    spdlog::logger logger("file_sink_test", sink);
    logger.info("first line");
    logger.flush();

    // The parent folder is created, and the file can be read while it is
    // still open for writing.
    REQUIRE(std::filesystem::exists(log_path));
    CHECK(ReadAll(log_path).find("first line") != std::string::npos);
  }

  {
    // A second sink appends instead of truncating.
    auto sink = rex::CreateFileSink(log_path);
    spdlog::logger logger("file_sink_test", sink);
    logger.info("second line");
    logger.flush();
  }
  const std::string text = ReadAll(log_path);
  CHECK(text.find("first line") != std::string::npos);
  CHECK(text.find("second line") != std::string::npos);

  std::filesystem::remove_all(dir, ec);
}
