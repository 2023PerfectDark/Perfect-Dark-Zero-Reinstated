#pragma once
#include <rex/cvar.h>

// Opt-in test bridge. Never expose a guest to solo or ordinary local play.
REXCVAR_DECLARE(bool, virtual_coop_guest);
namespace rex::system::xam {
void SetVirtualCoopGuest(bool enabled);
inline constexpr uint64_t kVirtualCoopXuid = 0xB13EBABEBABEBABF;
inline bool IsVirtualCoopGuest(uint32_t index) {
  return index == 1 && REXCVAR_GET(virtual_coop_guest);
}
}
