#pragma once
#include <cstdint>

namespace pdz_klobb_ui {
// These IDs are outside the retail localization banks and are intercepted by
// both string accessors before they can index the retail bank table.
inline constexpr uint32_t kNameText = 0x7F4B4C00;
inline constexpr uint32_t kDescriptionText = 0x7F4B4C01;

// Call on the guest update thread before exposing the custom weapon UI row.
// Native UI strings borrow these buffers, so storage lasts for the runtime.
bool RegisterText(uint8_t* base);
}
