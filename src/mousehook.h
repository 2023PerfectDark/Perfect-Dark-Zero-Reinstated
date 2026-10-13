#pragma once

#include <cstdint>
#include <rex/ui/window.h>

namespace pdz_input {
void Attach(rex::ui::Window* window);
void Detach();
void ValidateImage();
bool Supported();
struct Pointer { float x, y; uint64_t motion, click; bool active; };
Pointer MenuPointer();
void PointerAccept(uint64_t click, uint16_t key = 0x5800);
void InventoryMenuRendered();
bool CrosshairVisible();
bool ForceAimPose(uint32_t player);
void SetGameplayInputActive(bool active);
bool InventoryStorageFull(uint32_t inventory);
// Called only on the guest input polling thread, for guest user zero.
void Poll(uint8_t* state);
}

namespace pdz_input { bool MenuKeystroke(uint32_t user, uint32_t address); void PaceFrame(); }

namespace pdz_input { bool InventoryPageItem(uint32_t ui, uint32_t index, uint32_t& item); void ChangeInventoryPage(int direction); int InventoryPage(); int InventoryPages(); }
