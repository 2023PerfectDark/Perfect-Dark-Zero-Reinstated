#pragma once
/**
 * @file        rex/system/xex_identity.h
 * @brief       Fingerprint of the executable code in a XEX, so a title can
 *              tell at startup whether default.xex is the build its code was
 *              recompiled from
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <cstddef>
#include <cstdint>
#include <string>

#include <rex/image_info.h>
#include <rex/system/util/xex2_info.h>

namespace rex::runtime {

class XexModule;

static_assert(sizeof(xex2_page_descriptor) == 0x18, "XEX page descriptor layout");

/// FNV-1a 64 over raw bytes.
constexpr uint64_t Fnv1a64(const uint8_t* data, size_t size) {
  uint64_t hash = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < size; ++i) {
    hash = (hash ^ data[i]) * 0x100000001B3ull;
  }
  return hash;
}

/// Fingerprint of the page descriptor table in the XEX security header, as
/// stored (big-endian). Every entry holds the SHA1 of one block of the loaded
/// image, so two XEX files with the same fingerprint carry the same code
/// whatever their encryption, compression, media ID or region. A title update
/// rewrites the table, so a patched image has its own fingerprint.
inline uint64_t XexImageFingerprint(const xex2_page_descriptor* descriptors, uint32_t count) {
  if (!descriptors) {
    count = 0;
  }
  return Fnv1a64(reinterpret_cast<const uint8_t*>(descriptors),
                 size_t(count) * sizeof(xex2_page_descriptor));
}

/// Identity of a loaded XEX (title update applied, if there is one).
PPCImageIdentity ReadXexImageIdentity(const XexModule& xex);

enum class XexImageMatch {
  kNotRecorded,  ///< The generated code has no identity (generated before it was recorded)
  kMatch,
  kMismatch,
};

/// The code matches when the page descriptor tables match. Title ID and media
/// ID are not compared: another disc or region with the same code is fine.
constexpr XexImageMatch CompareXexImageIdentity(const PPCImageIdentity& expected,
                                                const PPCImageIdentity& loaded) {
  if (!expected.fingerprint && !expected.page_count) {
    return XexImageMatch::kNotRecorded;
  }
  return expected.fingerprint == loaded.fingerprint && expected.page_count == loaded.page_count
             ? XexImageMatch::kMatch
             : XexImageMatch::kMismatch;
}

/// "title 4D5307D3, media ID 750FF1D9, fingerprint A360A2573C02F664 (317 pages)"
std::string FormatXexImageIdentity(const PPCImageIdentity& identity);

}  // namespace rex::runtime
