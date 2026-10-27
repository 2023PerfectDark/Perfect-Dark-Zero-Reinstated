/**
 * @file        system/xex_identity_test.cpp
 * @brief       Tests for the XEX image fingerprint and the startup check's comparison
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <array>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include <rex/system/xex_identity.h>

using rex::PPCImageIdentity;
using rex::xex2_page_descriptor;
using rex::runtime::CompareXexImageIdentity;
using rex::runtime::Fnv1a64;
using rex::runtime::FormatXexImageIdentity;
using rex::runtime::XexImageFingerprint;
using rex::runtime::XexImageMatch;

namespace {

uint64_t HashString(const char* text) {
  return Fnv1a64(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
}

// Two descriptors as stored in a XEX: big-endian info word, then a SHA1.
std::array<uint8_t, 2 * sizeof(xex2_page_descriptor)> MakeTable() {
  std::array<uint8_t, 2 * sizeof(xex2_page_descriptor)> raw{};
  raw[3] = 0x11;
  for (int i = 0; i < 20; ++i) {
    raw[4 + i] = uint8_t(i);
  }
  raw[24] = 0x20;
  raw[27] = 0x01;
  for (int i = 0; i < 20; ++i) {
    raw[28 + i] = uint8_t(100 + i);
  }
  return raw;
}

uint64_t Fingerprint(const std::array<uint8_t, 2 * sizeof(xex2_page_descriptor)>& raw) {
  std::array<xex2_page_descriptor, 2> table;
  std::memcpy(table.data(), raw.data(), raw.size());
  return XexImageFingerprint(table.data(), uint32_t(table.size()));
}

}  // namespace

TEST_CASE("FNV-1a 64 matches the reference vectors", "[xex_identity]") {
  CHECK(HashString("") == 0xCBF29CE484222325ull);
  CHECK(HashString("a") == 0xAF63DC4C8601EC8Cull);
  CHECK(HashString("foobar") == 0x85944171F73967E8ull);
}

TEST_CASE("The fingerprint hashes the descriptor table as stored", "[xex_identity]") {
  auto raw = MakeTable();
  CHECK(Fingerprint(raw) == 0x4DC1519254B970D1ull);

  // One changed SHA1 byte is another image.
  raw[30] ^= 1;
  CHECK(Fingerprint(raw) != 0x4DC1519254B970D1ull);
}

TEST_CASE("An empty or missing table hashes to the FNV offset basis", "[xex_identity]") {
  CHECK(XexImageFingerprint(nullptr, 5) == 0xCBF29CE484222325ull);
  xex2_page_descriptor one{};
  CHECK(XexImageFingerprint(&one, 0) == 0xCBF29CE484222325ull);
}

TEST_CASE("Only the descriptor table decides the match", "[xex_identity]") {
  const PPCImageIdentity expected{0xA360A2573C02F664ull, 317, 0x4D5307D3, 0x750FF1D9};

  SECTION("same code from another disc or region") {
    PPCImageIdentity loaded = expected;
    loaded.media_id = 0x12345678;
    loaded.title_id = 0x4D5307D4;
    CHECK(CompareXexImageIdentity(expected, loaded) == XexImageMatch::kMatch);
  }
  SECTION("different code") {
    PPCImageIdentity loaded = expected;
    loaded.fingerprint ^= 1;
    CHECK(CompareXexImageIdentity(expected, loaded) == XexImageMatch::kMismatch);
  }
  SECTION("different table size") {
    PPCImageIdentity loaded = expected;
    loaded.page_count = 318;
    CHECK(CompareXexImageIdentity(expected, loaded) == XexImageMatch::kMismatch);
  }
  SECTION("code generated before the identity was recorded") {
    CHECK(CompareXexImageIdentity(PPCImageIdentity{}, expected) == XexImageMatch::kNotRecorded);
  }
}

TEST_CASE("The identity is printed as the log and the warning show it", "[xex_identity]") {
  const PPCImageIdentity identity{0xA360A2573C02F664ull, 317, 0x4D5307D3, 0x750FF1D9};
  CHECK(FormatXexImageIdentity(identity) ==
        "title 4D5307D3, media ID 750FF1D9, fingerprint A360A2573C02F664 (317 pages)");
}
