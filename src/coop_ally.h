#pragma once
#include <cstdint>
struct PPCContext;
namespace pdz_coop {
// Set only by the campaign mode menu, never by a launch flag.
void SelectBotCoop(bool enabled);
bool BotCoopSelected();
bool NativePartnerSelected();
void CharacterCreated(uint8_t* base, uint32_t actor, uint32_t setup);
void CharacterDestroyed(uint32_t actor);
void Tick(PPCContext& ctx, uint8_t* base);
void LevelEnded();
void PollRescueInput(bool useHeld);
bool PlayerDowned();
int RescueStatus();
int SyncStatus();
bool AssistDataThief(PPCContext& ctx, uint8_t* base, uint32_t puzzle, float seconds);
}
