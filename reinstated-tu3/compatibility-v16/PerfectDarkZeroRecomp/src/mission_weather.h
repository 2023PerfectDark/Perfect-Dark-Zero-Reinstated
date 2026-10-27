#pragma once
#include <cstdint>
#include <memory>
#include <rex/ui/immediate_drawer.h>
struct PPCContext;
namespace pdz_weather {
void Tick(PPCContext& ctx,uint8_t* base);
std::unique_ptr<rex::ui::ImmediateTexture> LoadSnowTexture(rex::ui::ImmediateDrawer* drawer);
void Draw(rex::ui::ImmediateTexture* snow_texture);
}
