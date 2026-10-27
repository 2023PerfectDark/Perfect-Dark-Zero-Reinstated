#include "generated/default/pdzstocktu3_init.h"
#include "game_cvars.h"
#include <rex/hook.h>
#include <string>
#include <algorithm>

REXCVAR_DEFINE_INT32(pdz_dub_language, 0, "Audio", "Voice language: 0 English, 1 Deutsch, 2 Japanese, 3 Latin American Spanish, 4 European Spanish, 5 French, 6 Italian. Reload mission after changing.").range(0, 6);
namespace { thread_local bool audioDirectory = false; }
REX_EXTERN(__imp__sub_8239CC98);
REX_EXTERN(__imp__sub_8239CEB8);

// This native resolver appends the localized folder to a supplied asset root.
// Restrict the override to AUDIO roots so strings and fonts remain unchanged.
REX_HOOK_RAW(sub_8239CC98) {
  // TU3 has a dedicated voice-root resolver; r4 is not a path argument.
  const bool previous=audioDirectory;
  audioDirectory=true;
  __imp__sub_8239CC98(ctx,base);
  audioDirectory=previous;
}
REX_HOOK_RAW(sub_8239CEB8) {
  if (audioDirectory) {
    constexpr unsigned ids[]={1,3,2,10,5,4,6};
    const int choice=std::clamp(REXCVAR_GET(pdz_dub_language),0,6);
    ctx.r3.u64=ids[choice];
  }
  __imp__sub_8239CEB8(ctx,base);
}
