/**
 * @file        system/xex_identity.cpp
 * @brief       Fingerprint of the executable code in a XEX
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/system/xex_identity.h>

#include <fmt/format.h>

#include <rex/system/xex_module.h>

namespace rex::runtime {

PPCImageIdentity ReadXexImageIdentity(const XexModule& xex) {
  PPCImageIdentity identity;
  const auto* security = xex.xex_security_info();
  identity.page_count = security->page_descriptor_count;
  identity.fingerprint =
      XexImageFingerprint(security->page_descriptors, security->page_descriptor_count);
  if (const auto* exec_info = xex.opt_execution_info()) {
    identity.title_id = uint32_t(exec_info->title_id);
    identity.media_id = uint32_t(exec_info->media_id);
  }
  return identity;
}

std::string FormatXexImageIdentity(const PPCImageIdentity& identity) {
  return fmt::format("title {:08X}, media ID {:08X}, fingerprint {:016X} ({} pages)",
                     identity.title_id, identity.media_id, identity.fingerprint,
                     identity.page_count);
}

}  // namespace rex::runtime
