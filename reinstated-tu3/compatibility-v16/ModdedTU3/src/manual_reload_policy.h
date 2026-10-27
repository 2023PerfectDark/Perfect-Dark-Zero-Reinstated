#pragma once
#include <cstdint>

namespace pdz_reload {
// TU3 common gun update: magazine == 0, reserve != 0, reload ready.
// Other callers query the same native predicate for unrelated actions.
constexpr bool SuppressAutomatic(bool enabled, bool local,
                                 uint32_t return_address) {
  return enabled && local && return_address == 0x8223A4A8;
}
}
