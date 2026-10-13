#include "generated/default/perfectdarkzerorecomp_init.h"
#include "campaign_menu.h"
#include "mousehook.h"
#include "utils.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/ui/image_decode.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr unsigned kSize = 1024;
constexpr uint32_t kFormat = 0x18280086; // Native linear A8R8G8B8, used by XUI.
constexpr std::array<const char*, 4> names{"modern", "original", "classic", "surface"};
// Keep a reference only while the style menu is open. Holding these allocations
// across a mission load takes memory away from the native package loader.
std::array<uint32_t, 3> textures{};

uint32_t LoadPortrait(PPCContext& call, uint8_t* base, unsigned style) {
  const bool mission_card=style==3;
  if (!mission_card&&textures[style]) return textures[style];
  const auto path = mission_card?
      utils::RepoRoot()/"mod_assets/surface/mission-placeholder.png":
      utils::RepoRoot()/"mod_assets/campaign"/(std::string(names[style])+".png");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return 0;
  const auto size = input.tellg();
  if (size <= 0 || size > 20 * 1024 * 1024) return 0;
  std::vector<uint8_t> encoded(static_cast<size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(encoded.data()), size)) return 0;
  int width = 0, height = 0;
  auto pixels = rex::ui::DecodeImageRGBA(encoded.data(), encoded.size(), width, height);
  if (pixels.empty() || width <= 0 || height <= 0 || width > 4096 || height > 4096)
    return 0;

  // Fit the alpha silhouette into the existing 1024-square native portrait
  // element. Its position, animation, depth and scanline layers remain native.
  int left = width, top = height, right = -1, bottom = -1;
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    if (pixels[(size_t(y) * width + x) * 4 + 3] < 8) continue;
    left = std::min(left, x); top = std::min(top, y);
    right = std::max(right, x); bottom = std::max(bottom, y);
  }
  if (right < left || bottom < top) return 0;
  const float scale = mission_card?std::max(float(kSize)/width,float(kSize)/height):
      std::min(640.f / (right - left + 1), 640.f / (bottom - top + 1));
  const float draw_width = (right - left + 1) * scale;
  const float start_x = (mission_card?512.f:600.f) - draw_width * .5f;
  const float start_y = mission_card?(kSize-height*scale)*.5f:224.f;

  call.r3.u64 = kSize; call.r4.u64 = kSize; call.r5.u64 = 1;
  call.r6.u64 = 1; call.r7.u64 = 0; call.r8.u64 = kFormat;
  call.r9.u64 = 1; call.r10.u64 = 3;
  sub_826DD008(call, base); // Native texture allocation, including GPU memory.
  const uint32_t texture = call.r3.u32;
  if (!texture) return 0;
  const uint32_t locked = call.r1.u32 + 80;
  REX_STORE_U32(locked, 0); REX_STORE_U32(locked + 4, 0);
  call.r3.u64 = texture; call.r4.u64 = 0; call.r5.u64 = locked;
  call.r6.u64 = 0; call.r7.u64 = 0;
  sub_826DCD48(call, base); // Texture LockRect(0, ...).
  const uint32_t pitch = REX_LOAD_U32(locked), data = REX_LOAD_U32(locked + 4);
  if (!data || pitch < kSize * 4 || pitch > kSize * 16) {
    call.r3.u64 = texture; sub_826D2208(call, base);
    return 0;
  }
  // LockRect may return a physical-heap alias with a host offset. The GPU
  // reads the translated allocation, not raw base + guest address.
  auto* destination = rex::Runtime::instance()->memory()->TranslateVirtual<uint8_t*>(data);
  for (unsigned y = 0; y < kSize; ++y) for (unsigned x = 0; x < kSize; ++x) {
    const float sx = (float(x) - start_x + .5f) / scale + left - .5f;
    const float sy = (float(y) - start_y + .5f) / scale + top - .5f;
    uint32_t argb = 0;
    if (sx >= left && sx <= right && sy >= top && sy <= bottom) {
      const int x0 = int(sx), y0 = int(sy);
      const int x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
      const float fx = sx - x0, fy = sy - y0;
      std::array<float, 4> rgba{};
      for (unsigned c = 0; c < 4; ++c) {
        const auto at = [&](int px, int py) { return pixels[(size_t(py) * width + px) * 4 + c]; };
        rgba[c] = std::lerp(std::lerp(float(at(x0,y0)), float(at(x1,y0)), fx),
                            std::lerp(float(at(x0,y1)), float(at(x1,y1)), fx), fy);
      }
      const float light = .3f + .7f * (rgba[0] * .299f + rgba[1] * .587f + rgba[2] * .114f) / 255.f;
      // Match the game's blue hologram palette. Transparency remains per pixel.
      const uint32_t alpha = uint32_t(rgba[3] * .85f);
      argb = (alpha << 24) | (uint32_t(90 * light) << 16) |
             (uint32_t(175 * light) << 8) | uint32_t(255 * light);
      if(mission_card)argb=0xFF000000u|(uint32_t(rgba[0])<<16)|
          (uint32_t(rgba[1])<<8)|uint32_t(rgba[2]);
    }
    auto* pixel = destination + y * pitch + x * 4;
    pixel[0] = uint8_t(argb >> 24); pixel[1] = uint8_t(argb >> 16);
    pixel[2] = uint8_t(argb >> 8); pixel[3] = uint8_t(argb);
  }
  call.r3.u64 = texture; call.r4.u64 = 0;
  sub_826DCD60(call, base); // Unlock/flush through the native texture routine.
  // The card transfers its sole reference to XUI. No retained allocation may
  // survive menu teardown and compete with the mission package loader.
  if(!mission_card)textures[style] = texture;
  REXLOG_INFO("PDZ campaign portrait loaded: {} ({}x{})", names[style], width, height);
  return texture;
}
}

namespace pdz_campaign {
void ReleasePortraits(PPCContext& call, uint8_t* base) {
  for (unsigned style = 0; style < textures.size(); ++style) {
    const uint32_t texture = textures[style];
    if (!texture) continue;
    textures[style] = 0;
    call.r3.u64 = texture;
    sub_826D2208(call, base);
    REXLOG_INFO("PDZ campaign portrait released: {} (remaining references {})",
                names[style], call.r3.u32);
  }
}
}

REX_EXTERN(__imp__sub_82398470);
REX_HOOK_RAW(sub_82398470) {
  int style = -1;
  if (pdz_input::Supported() && ctx.r4.u32) {
    // XUI passes its own valid UTF-16 resource name to this registered callback.
    std::u16string path;
    for (unsigned i = 0; i < 128; ++i) {
      const auto c = REX_LOAD_U16(ctx.r4.u32 + i * 2);
      if (!c) break;
      path += char16_t(c);
    }
    constexpr std::array<std::u16string_view, 4> suffixes{
        u"pdz_style_modern", u"pdz_style_original", u"pdz_style_classic",u"pdz_surface_card"};
    for (unsigned i = 0; i < suffixes.size(); ++i)
      if (path.ends_with(suffixes[i])) style = int(i);
  }
  if (style < 0) { __imp__sub_82398470(ctx, base); return; }
  auto call = ctx; call.r1.u32 -= 1536;
  const auto texture = LoadPortrait(call, base, unsigned(style));
  if (texture) {
    if(style!=3){call.r3.u64 = texture; sub_826D21A0(call, base);}
    const uint32_t info = ctx.r5.u32;
    REX_STORE_U32(info, kSize); REX_STORE_U32(info + 4, kSize);
    REX_STORE_U32(info + 8, 32); REX_STORE_U32(info + 12, 1);
    REX_STORE_U32(info + 16, kFormat); REX_STORE_U32(info + 20, 3);
    REX_STORE_U32(info + 24, 0); REX_STORE_U32(ctx.r6.u32, texture);
    ctx.r3.u64 = 0;
    return;
  }
  REXLOG_WARN("PDZ campaign portrait missing/unreadable: {}; using original", names[style]);
  const std::u16string_view fallback = style==3?
      u"DarkXUIAsset://aid_texture_darkxui_frontend_info_level0":
      u"DarkXUIAsset://aid_texture_darkxui_frontend_info_missions_jo_holo";
  const uint32_t text = call.r1.u32 + 256;
  for (unsigned i = 0; i <= fallback.size(); ++i)
    REX_STORE_U16(text + i * 2, i == fallback.size() ? 0 : fallback[i]);
  call.r3 = ctx.r3; call.r4.u64 = text; call.r5 = ctx.r5; call.r6 = ctx.r6;
  __imp__sub_82398470(call, base);
  ctx.r3 = call.r3;
}
