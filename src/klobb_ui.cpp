#include "generated/default/perfectdarkzerorecomp_init.h"
#include "klobb_ui.h"
#include "mousehook.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <atomic>
#include <string_view>

REX_EXTERN(__imp__sub_82393AD0);
REX_EXTERN(__imp__sub_82393B30);

namespace {
constexpr std::u16string_view kName = u"KLOBB";
constexpr std::u16string_view kDescription =
    u"A compact automatic weapon. Available for this mission only.";
constexpr uint32_t kDescriptionOffset = (kName.size() + 1) * 2;
std::atomic<uint32_t> text_storage{0};

bool CustomText(uint32_t id) {
  return id == pdz_klobb_ui::kNameText ||
         id == pdz_klobb_ui::kDescriptionText;
}

void WriteText(uint8_t* base, uint32_t address, std::u16string_view text) {
  for (uint32_t i = 0; i < text.size(); ++i)
    REX_STORE_U16(address + i * 2, text[i]);
  REX_STORE_U16(address + text.size() * 2, 0);
}
}

namespace pdz_klobb_ui {
bool RegisterText(uint8_t* base) {
  if (text_storage.load(std::memory_order_acquire)) return true;
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !base || !pdz_input::Supported()) return false;
  const auto storage = runtime->memory()->SystemHeapAlloc(
      kDescriptionOffset + (kDescription.size() + 1) * 2, 16);
  if (!storage) return false;
  WriteText(base, storage, kName);
  WriteText(base, storage + kDescriptionOffset, kDescription);
  // 826400B8 creates borrowed native strings, so these buffers must outlive
  // level/weapon-manager teardown and any UI element retaining their pointers.
  text_storage.store(storage, std::memory_order_release);
  return true;
}
}

REX_HOOK_RAW(sub_82393AD0) {
  const auto id = ctx.r3.u32;
  if (CustomText(id)) {
    const auto storage = text_storage.load(std::memory_order_acquire);
    ctx.r3.u64 = storage ? storage +
        (id == pdz_klobb_ui::kNameText ? 0 : kDescriptionOffset) : 0;
    return;
  }
  __imp__sub_82393AD0(ctx, base);
}

REX_HOOK_RAW(sub_82393B30) {
  const auto id = ctx.r3.u32;
  if (CustomText(id)) {
    ctx.r3.s64 = text_storage.load(std::memory_order_acquire) ?
        static_cast<int64_t>(id == pdz_klobb_ui::kNameText ?
            kName.size() : kDescription.size()) : -1;
    return;
  }
  __imp__sub_82393B30(ctx, base);
}
