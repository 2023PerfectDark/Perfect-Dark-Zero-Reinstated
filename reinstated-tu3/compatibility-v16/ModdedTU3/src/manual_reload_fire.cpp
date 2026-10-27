#include "generated/default/pdzstocktu3_init.h"
#include "game_cvars.h"
#include "mousehook.h"

// Called only from the empty-magazine branch of the native Fire action.
// Explicit reload requests and the already-reloading state bypass this gate.
bool pdz_block_empty_fire_reload(uint8_t* base, uint32_t action) {
  if (!pdz_input::Supported() || !REXCVAR_GET(pdz_manual_reload)) return false;
  const auto weapon = REX_LOAD_U32(action + 28);
  const auto local = REX_LOAD_U32(0x82E36228);
  return local && weapon &&
      REX_LOAD_U32(weapon + 92) == REX_LOAD_U32(local + 964);
}
