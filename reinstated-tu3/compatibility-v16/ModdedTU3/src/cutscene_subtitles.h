#pragma once
#include <cstdint>
struct ImFontAtlas;
namespace pdz_subtitles { void Initialize(ImFontAtlas* atlas); void Tick(uint8_t* base, bool active); void Draw(); }
