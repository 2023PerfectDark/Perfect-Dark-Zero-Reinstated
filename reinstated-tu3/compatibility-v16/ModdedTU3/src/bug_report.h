#pragma once
#include <memory>
#include <functional>
namespace rex::ui { class ImGuiDialog; class ImGuiDrawer; class Presenter; }
namespace pdz_report {
void Open();
bool IsOpen();
std::unique_ptr<rex::ui::ImGuiDialog> MakeDialog(rex::ui::ImGuiDrawer*, std::function<rex::ui::Presenter*()>);
}


