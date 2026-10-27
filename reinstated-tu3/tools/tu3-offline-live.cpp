// TU3 wrapper 82775888 sends XLIVEBASE 5800E every update. The runtime
// leaves its stack output zero, returns X_E_FAIL, and logs each request.
// The wrapper ignores that HRESULT and returns the zero output. Preserve that
// exact observable result without dispatching or flushing an error each frame.
#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <mutex>
REX_HOOK_RAW(sub_82775888) {
  static std::once_flag notice;
  std::call_once(notice,[]{REXLOG_INFO("TU3 offline LIVE query: bypassing unsupported 5800E polling; preserving zero result");});
  ctx.r3.u64=0;
}
