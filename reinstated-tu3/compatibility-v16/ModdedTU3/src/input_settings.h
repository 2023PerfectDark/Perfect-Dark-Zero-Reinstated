#pragma once
#include "game_cvars.h"
#include "mousehook.h"
#include "utils.h"
#include "local_profiles.h"
#include "surface_runtime.h"
#include <rex/ui/imgui_dialog.h>
#include <imgui.h>

class PdzInputSettings final : public rex::ui::ImGuiDialog {
 public:
  explicit PdzInputSettings(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}
 protected:
  void OnDraw(ImGuiIO&) override {
    ImGui::SetNextWindowSize(ImVec2(650, 540), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("PDZ mouse and FOV (F6 to close)")) {
      ImGui::SetWindowFontScale(1.5f);
      DrawContents(saved_);
    }
    ImGui::End();
  }
 public:
  static void DrawContents(bool& saved_) {
      int dub = REXCVAR_GET(pdz_dub_language);
      if (ImGui::Combo("Voice language", &dub, "English\0Deutsch\0Japanese\0Spanish (Latin America)\0Spanish (Europe)\0French\0Italian\0")) {
        REXCVAR_SET(pdz_dub_language, dub);
        rex::cvar::SaveConfig(utils::SettingsDir() / "hardware.toml");
      }
      ImGui::TextWrapped("Reload the mission after changing voices. Menu and subtitle language is unchanged.");
      ImGui::TextWrapped("I: random weapon inspection (idle animations or bolt motion).");
      if (!pdz_input::Supported())
        ImGui::TextWrapped("Waiting for supported retail TU0 image. Camera/FOV writes are disabled until verified.");
      bool enabled = REXCVAR_GET(pdz_mousehook);
      if (ImGui::Checkbox("Direct mouse look", &enabled)) REXCVAR_SET(pdz_mousehook, enabled);
      float sensitivity = float(REXCVAR_GET(pdz_mouse_sensitivity));
      if (ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 10.f, "%.2f"))
        REXCVAR_SET(pdz_mouse_sensitivity, double(sensitivity));
      bool invert_x = REXCVAR_GET(pdz_mouse_invert_x);
      bool invert_y = REXCVAR_GET(pdz_mouse_invert_y);
      if (ImGui::Checkbox("Invert X", &invert_x)) REXCVAR_SET(pdz_mouse_invert_x, invert_x);
      ImGui::SameLine();
      if (ImGui::Checkbox("Invert Y", &invert_y)) REXCVAR_SET(pdz_mouse_invert_y, invert_y);
      float fov = float(REXCVAR_GET(pdz_fov));
      if (ImGui::SliderFloat("FOV scale", &fov, 40.f, 120.f, "%.0f"))
        REXCVAR_SET(pdz_fov, double(fov));
      ImGui::TextWrapped("Original: 58. Your Xenia patch: 100. Changes apply on the next game input poll.");
      if (ImGui::Button("Original FOV")) REXCVAR_SET(pdz_fov, 58.0);
      ImGui::SameLine();
      if (ImGui::Button("Xenia FOV (100)")) REXCVAR_SET(pdz_fov, 100.0);
      ImGui::Separator();
      if (ImGui::Button("Local player profiles (F9)")) pdz_profiles::Toggle();
      if (ImGui::Button("Replay Surface intro (test)")) pdz_surface::RequestIntroReplay();
      ImGui::TextWrapped("Surface test only: close F6 after clicking to inspect the camera sequence.");
      ImGui::Separator();
      ImGui::TextUnformatted("Right-click + wheel adjusts aim zoom (fixed).");
      int pressure = REXCVAR_GET(pdz_ads_pressure);
      if (ImGui::SliderInt("Aim trigger pressure", &pressure, 32, 255))
        REXCVAR_SET(pdz_ads_pressure, pressure);
      ImGui::TextWrapped("Wheel up pulls the aim trigger farther; down releases it partway. 255 is full pull. While dual wielding, right-click always fully pulls the left gun's trigger.");
      bool all_ads = REXCVAR_GET(pdz_all_weapon_ads);
      if (ImGui::Checkbox("Experimental ADS pose for all weapons", &all_ads))
        REXCVAR_SET(pdz_all_weapon_ads, all_ads);
      ImGui::TextWrapped("Hold RMB and scroll down to pressure 32. Uses each weapon's native aim-position offsets without extra camera zoom. Untested models may not align their sights; disable this option to restore normal poses.");
      bool menus = REXCVAR_GET(pdz_mouse_menu_navigation);
      if (ImGui::Checkbox("Mouse menu navigation", &menus))
        REXCVAR_SET(pdz_mouse_menu_navigation, menus);
      bool forced = REXCVAR_GET(pdz_menu_mode);
      if (ImGui::Checkbox("Force menu mode (F8)", &forced)) REXCVAR_SET(pdz_menu_mode, forced);
      ImGui::TextWrapped("Menus: point at a button and left-click to select it. Right-click goes back; middle-click sends Start. Wheel navigation remains available. F8 switches pause menus between navigation and camera look.");
      bool cap = REXCVAR_GET(pdz_cutscene_60fps);
      if (ImGui::Checkbox("Menu / scripted camera cap (60 FPS)", &cap)) REXCVAR_SET(pdz_cutscene_60fps, cap);
      ImGui::TextWrapped("Also caps front-end menus and other control-locked states. Normal gameplay keeps its existing frame limit.");
      bool inventory = REXCVAR_GET(pdz_ignore_weapon_size);
      bool refill = REXCVAR_GET(pdz_training_ammo_refill);
      if (ImGui::Checkbox("Cheat: DataCore training ammo refill", &refill))
        REXCVAR_SET(pdz_training_ammo_refill, refill);
      ImGui::TextWrapped("Classic: fixed weapon spread. Modern: gradual hip-fire spread, tighter zoom aiming.");
      bool zoom_auto = REXCVAR_GET(pdz_campaign_zoom_auto);
      if (ImGui::Checkbox("Modern/Classic: full auto while zoomed", &zoom_auto))
        REXCVAR_SET(pdz_campaign_zoom_auto, zoom_auto);
      bool holsters = REXCVAR_GET(pdz_visible_holsters);
      bool gore = REXCVAR_GET(pdz_campaign_gore);
      if (ImGui::Checkbox("Campaign sniper headshot gore", &gore))
        REXCVAR_SET(pdz_campaign_gore, gore);
      int gore_minutes = REXCVAR_GET(pdz_gore_decal_minutes);
      if (ImGui::SliderInt("Gore lifetime (minutes)", &gore_minutes, 1, 20))
        REXCVAR_SET(pdz_gore_decal_minutes, gore_minutes);
      ImGui::TextWrapped("Single-player only. Default: 10 minutes. Applies to blood marks and newly spawned fragments. Blood marks are capped at 160; lowering the lifetime also clears older marks sooner.");
      bool body = REXCVAR_GET(pdz_first_person_body);
      if (ImGui::Checkbox("First-person body (experimental)", &body))
        REXCVAR_SET(pdz_first_person_body, body);
      if (ImGui::Checkbox("Visible holstered weapons", &holsters))
        REXCVAR_SET(pdz_visible_holsters, holsters);
      if (ImGui::Checkbox("Ignore weapon size (experimental)", &inventory))
        REXCVAR_SET(pdz_ignore_weapon_size, inventory);
      ImGui::TextWrapped("Removes the four-slot size restriction for the local player. The game's 13-object storage limit still applies, including non-weapon items. Disable before starting a fresh mission to return to normal capacity.");
      ImGui::Text("Inventory page %d / %d (Page Up / Page Down)", pdz_input::InventoryPage(), pdz_input::InventoryPages());
      if (ImGui::Button("Previous inventory page")) pdz_input::ChangeInventoryPage(-1);
      ImGui::SameLine();
      if (ImGui::Button("Next inventory page")) pdz_input::ChangeInventoryPage(1);
      if (ImGui::Button("Save settings")) {
        rex::cvar::SaveConfig(utils::SettingsDir() / "hardware.toml");
        saved_ = true;
      }
      if (saved_) ImGui::TextUnformatted("Settings saved.");
      ImGui::TextWrapped("WASD move | LMB fire | RMB aim | E interact | R reload | Space LB | C crouch | F/B B | 1 weapon");
  }
 private:
  bool saved_ = false;
};
