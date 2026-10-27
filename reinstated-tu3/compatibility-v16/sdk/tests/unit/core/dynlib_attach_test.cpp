/**
 * @file        core/dynlib_attach_test.cpp
 * @brief       Tests for DynamicLibrary::Attach (take a loaded library, never load one)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <rex/platform/dynlib.h>

namespace {

// A library every test process has loaded, and a symbol it exports.
#if defined(_WIN32)
constexpr const char* kLoadedLibrary = "kernel32.dll";
constexpr const char* kLoadedSymbol = "GetCurrentProcessId";
constexpr const char* kMissingLibrary = "rexglue_no_such_library_7f3a.dll";
#elif defined(__APPLE__)
constexpr const char* kLoadedLibrary = "/usr/lib/libSystem.B.dylib";
constexpr const char* kLoadedSymbol = "malloc";
constexpr const char* kMissingLibrary = "librexglue_no_such_library_7f3a.dylib";
#else
constexpr const char* kLoadedLibrary = "libc.so.6";
constexpr const char* kLoadedSymbol = "malloc";
constexpr const char* kMissingLibrary = "librexglue_no_such_library_7f3a.so";
#endif

}  // namespace

TEST_CASE("DynamicLibrary::Attach takes a library that is already loaded", "[dynlib]") {
  rex::platform::DynamicLibrary library;
  REQUIRE(library.Attach(kLoadedLibrary));
  CHECK(static_cast<bool>(library));
  CHECK(library.last_error().empty());
  CHECK(library.GetRawSymbol(kLoadedSymbol) != nullptr);
  library.Close();
  CHECK_FALSE(static_cast<bool>(library));
}

TEST_CASE("DynamicLibrary::Attach fails for a library that is not loaded", "[dynlib]") {
  rex::platform::DynamicLibrary library;
  CHECK_FALSE(library.Attach(kMissingLibrary));
  CHECK_FALSE(static_cast<bool>(library));
  CHECK_FALSE(library.last_error().empty());
}
