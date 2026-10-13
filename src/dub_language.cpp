#include "generated/default/perfectdarkzerorecomp_init.h"
#include "game_cvars.h"
#include <rex/hook.h>
#include <string>
#include <algorithm>

REXCVAR_DEFINE_INT32(pdz_dub_language, 0, "Audio", "Voice language: 0 English, 1 Deutsch, 2 Japanese, 3 Latin American Spanish, 4 European Spanish, 5 French, 6 Italian. Reload mission after changing.").range(0, 6);
namespace { thread_local bool audioDirectory = false; }
REX_EXTERN(__imp__sub_823948D0);
REX_EXTERN(__imp__sub_82394A40);

// This native resolver appends the localized folder to a supplied asset root.
// Restrict the override to AUDIO roots so strings and fonts remain unchanged.
REX_HOOK_RAW(sub_823948D0) {
  std::string root;
  const auto address = ctx.r4.u32;
  if (address && address < 0xFFFFFF00u) {
    for (unsigned i=0;i<256;++i) {
      const char c=char(REX_LOAD_U8(address+i));
      if (!c) break;
      root.push_back(c >= 'A' && c <= 'Z' ? c+('a'-'A') : c);
    }
  }
  const bool previous=audioDirectory;
  audioDirectory=root.find("audio\\")!=std::string::npos || root.find("audio/")!=std::string::npos;
  __imp__sub_823948D0(ctx,base);
  audioDirectory=previous;
}
REX_HOOK_RAW(sub_82394A40) {
  if (audioDirectory) {
    constexpr unsigned ids[]={1,3,2,10,5,4,6};
    const int choice=std::clamp(REXCVAR_GET(pdz_dub_language),0,6);
    ctx.r3.u64=ids[choice];
  }
  __imp__sub_82394A40(ctx,base);
}
