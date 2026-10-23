#pragma once

#include <memory>
#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/keybinds.h>
#include "game_cvars.h"

class FovSettings final : public rex::ui::ImGuiDialog
{
public:
  explicit FovSettings(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

protected:
  void OnDraw(ImGuiIO &) override
  {
    ImGui::SetNextWindowSize(ImVec2(480, 180), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Field of view (F6 to close)", nullptr, ImGuiWindowFlags_NoCollapse))
    {
      float fov = static_cast<float>(REXCVAR_GET(pdz_fov));
      if (ImGui::SliderFloat("Vertical FoV", &fov, 40.0f, 120.0f, "%.0f degrees",
                             ImGuiSliderFlags_AlwaysClamp))
      {
        REXCVAR_SET(pdz_fov, static_cast<double>(fov));
      }
      if (ImGui::Button("Reset to original (58)"))
      {
        REXCVAR_SET(pdz_fov, 58.0);
      }
      ImGui::TextWrapped("Updates on the next gameplay camera frame. To keep this value, "
                         "use Save to config in the F4 Settings overlay.");
    }
    ImGui::End();
  }
};
