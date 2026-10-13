// perfectdarkzerorecomp - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/system/gpu_plugin.h>

#include "debug_tools.h"
#include "game_cvars.h"
#include "game_patches.h"
#include "mousehook.h"
#include "input_settings.h"
#include "pd_menu_overlay.h"
#include "local_profiles.h"
#include <rex/ui/keybinds.h>
#include <rex/input/input_system.h>
#include "utils.h"
#include "mod/third_person.h"
#include "achievement_popup.h"
#include "coop_ally.h"

#ifdef REXGLUE_ENABLE_PERF_COUNTERS
#include <rex/perf/counter.h>
#endif

// A non-interactive cinematic curtain. No ImGui window is created, so it
// neither captures input nor changes the native mission/menu focus.
class SurfaceIntroFade final : public rex::ui::ImGuiDialog {
 public:
  explicit SurfaceIntroFade(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}
 private:
  void OnDraw(ImGuiIO&) override {
    if(const int status=pdz_coop::SyncStatus()) {
      const char* text=status==2?"ALLY READY - fire for synchronized headshots":"ALLY MOVING INTO POSITION";
      const auto* viewport=ImGui::GetMainViewport();
      const float size=std::max(18.f,viewport->Size.y*.023f);
      const auto extent=ImGui::GetFont()->CalcTextSizeA(size,10000.f,0.f,text);
      const ImVec2 p(viewport->Pos.x+(viewport->Size.x-extent.x)*.5f,
                     viewport->Pos.y+viewport->Size.y*.76f);
      ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(),size,{p.x+1,p.y+1},IM_COL32(0,0,0,230),text);
      ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(),size,p,
          status==2?IM_COL32(100,240,190,255):IM_COL32(220,225,235,255),text);
    }
    const int caption=pdz_surface::IntroCaption();
    if(caption){
      const auto* viewport=ImGui::GetMainViewport();
      const char* text=caption==1?"dataDyne Arctic Research Station":"3 weeks later";
      const float size=viewport->Size.y*.055f;
      const ImVec2 position(viewport->Pos.x+viewport->Size.x*.07f,
          viewport->Pos.y+viewport->Size.y*.875f);
      auto* draw=ImGui::GetForegroundDrawList();
      const float outline=std::max(1.f,size*.045f);
      for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)if(x||y)
        draw->AddText(ImGui::GetFont(),size,ImVec2(position.x+x*outline,position.y+y*outline),
            IM_COL32(20,20,20,255),text);
      draw->AddText(ImGui::GetFont(),size,position,IM_COL32(255,255,255,255),text);
    }
    if(!pdz_surface::IntroCameraActive() && pdz_surface::ObjectiveNotificationSeconds()>0.f){
      const auto* viewport=ImGui::GetMainViewport();
      const char* text="OBJECTIVE B COMPLETED - Obtain safe key";
      const float size=std::max(20.f,viewport->Size.y*.027f);
      const auto extent=ImGui::GetFont()->CalcTextSizeA(size,10000.f,0.f,text);
      const ImVec2 position(viewport->Pos.x+(viewport->Size.x-extent.x)*.5f,viewport->Pos.y+viewport->Size.y*.18f);
      auto* draw=ImGui::GetForegroundDrawList();
      draw->AddRectFilled(ImVec2(position.x-16,position.y-10),ImVec2(position.x+extent.x+16,position.y+extent.y+10),IM_COL32(5,20,35,210),4.f);
      draw->AddText(ImGui::GetFont(),size,position,IM_COL32(120,255,150,255),text);
    }
    const float opacity=pdz_surface::IntroFadeOpacity();
    if(opacity<=0.f)return;
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::GetForegroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y),
        IM_COL32(0,0,0,int(opacity*255.f+.5f)));
  }
};

class PerfectdarkzerorecompApp : public rex::ReXApp
{
public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext &ctx)
  {
    return std::unique_ptr<PerfectdarkzerorecompApp>(new PerfectdarkzerorecompApp(ctx, "perfectdarkzerorecomp",
                                                                                  PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig &config) override
  {
    std::string backend = REXCVAR_GET(graphics_backend);
    if (backend != "any" && !config.gpu_plugin.empty())
    {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      if (!config.graphics)
      {
        REXLOG_WARN("graphics_backend '{}' unavailable, falling back to automatic selection",
                    backend);
      }
    }
  }

  // void OnLoadXexImage(std::string& xex_image) override {}
  void OnPostLoadXexImage() override
  {
    game_patches::ApplyEnabledPatches();
  }

  void OnPostSetup() override
  {
    // Read-only mod models live outside user settings and base-game assets.
    // This location is supported by the original in-game updater as well.
    const auto models = utils::RepoRoot() / "mod_assets/models";
    if (std::filesystem::is_directory(models)) {
      constexpr auto mount = "\\Device\\PdzModModels";
      auto device = std::make_unique<rex::filesystem::HostPathDevice>(mount, models, true);
      if (device->Initialize() && runtime()->file_system()->RegisterDevice(std::move(device)))
        runtime()->file_system()->RegisterSymbolicLink("klobb:", mount);
    }
    utils::LoadSettingsFiles();
    pdz_input::Attach(window());
    if (auto* input = runtime()->input_system()) {
      static_cast<rex::input::InputSystem*>(input)->SetActiveCallback([this] {
        const bool active = !input_settings_ && !pdz_profiles::IsOpen() &&
            ((!imgui_drawer() || !imgui_drawer()->HasDialogs()) ||
             !imgui_drawer()->GetIO().WantCaptureMouse);
        pdz_input::SetGameplayInputActive(active);
        return active;
      });
    }

#ifdef REXGLUE_ENABLE_PERF_COUNTERS
    // SDK exposes the perf_log_csv cvar but never wires it up; do it here so
    // --perf_log_csv=<path> actually produces per-frame CSV output.
    // Use GetFlagByName to avoid needing a cross-DLL declaration of the cvar.
    rex::perf::SetCsvLogPath(rex::cvar::GetFlagByName("perf_log_csv"));
#endif

    // Run debug tools only if enabled via hardware.toml (dev_debug_runtime = true)
    if (REXCVAR_GET(dev_debug_runtime))
    {
      debug_tools::PerformMissingFunctionScan();
      debug_tools::PerformStubSweep();
    }
  }

  void OnShutdown() override
  {
    pdz_input::Detach();
    rex::ui::UnregisterBind("bind_pdz_input");
    rex::ui::UnregisterBind("bind_pdz_menu_mode");
    rex::ui::UnregisterBind("bind_pdz_profiles");
    pd_menu_.reset();
    profiles_.reset();
    surface_fade_.reset();
    input_settings_.reset();
#ifdef REXGLUE_ENABLE_PERF_COUNTERS
    rex::perf::FlushCsv();
#endif
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override
  {
    third_person::RegisterBinds();
    pd_menu_=std::make_unique<pdz_menu::OverlayDialog>(drawer);
    surface_fade_=std::make_unique<SurfaceIntroFade>(drawer);
    profiles_ = pdz_profiles::MakeProfileDialog(drawer);
    rex::ui::RegisterBind("bind_pdz_profiles", "F9", "Local player profiles", [] {
      pdz_profiles::Toggle();
    });
    rex::ui::RegisterBind("bind_pdz_menu_mode", "F8", "Toggle mouse menu navigation", [] {
      REXCVAR_SET(pdz_menu_mode, !REXCVAR_GET(pdz_menu_mode));
    });
    rex::ui::RegisterBind("bind_pdz_input", "F6", "PDZ mouse and FOV settings", [this, drawer] {
      if (input_settings_) input_settings_.reset();
      else input_settings_ = std::make_unique<PdzInputSettings>(drawer);
    });
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override
  {
    pdz_menu::BitmapFont::PrepareAtlas(atlas);
  }

  void OnConfigurePaths(rex::PathConfig &paths) override
  {
    // --game_data_root / REX_GAME_DATA_ROOT still win: this cvar is read
    // before any config file loads, so it's already non-empty here if set.
    if (paths.game_data_root.empty())
    {
      paths.game_data_root = utils::RepoRoot() / "assets";
    }

    // Hardware/rendering settings become the SDK's primary config file, so
    // the in-game Settings overlay's "Save to config" also lands here.
    paths.config_path = utils::SettingsDir() / "hardware.toml";

    // Input mapping lives in its own file, loaded manually below (the SDK
    // only auto-loads the single path above).
    utils::LoadSettingsFiles();
    if (REXCVAR_GET(pdz_coop_native_partner)) {
      // Native-join development runs use their own saves, including Player 1.
      paths.user_data_root = utils::RepoRoot() / "native-coop-userdata";
      paths.cache_root = paths.user_data_root / "cache";
    }
    pdz_profiles::Configure(paths.user_data_root);
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog() override {
    return std::make_unique<PdzAchievementPopup>(imgui_drawer(), achievements(), immediate_drawer());
  }
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
private:
  std::unique_ptr<SurfaceIntroFade> surface_fade_;
  std::unique_ptr<PdzInputSettings> input_settings_;
  std::unique_ptr<rex::ui::ImGuiDialog> pd_menu_;
  std::unique_ptr<rex::ui::ImGuiDialog> profiles_;
};




