#pragma once
#include <cstdint>
struct PPCContext;
namespace pdz_surface {
bool AllocatingCabinAtlas();
void Tick(PPCContext& ctx,uint8_t* base);
bool Selected(uint8_t* base);
void HandleHostCutscene(PPCContext& ctx,uint8_t* base,uint32_t object);
void RequestIntroReplay();
void RequestIntroTitleSkip();
float IntroFadeOpacity();
float ObjectiveNotificationSeconds();
bool EndingCameraActive();
bool IntroCameraActive();
void RefreshIntroProp(PPCContext& ctx,uint8_t* base);
int IntroCaption();
}
