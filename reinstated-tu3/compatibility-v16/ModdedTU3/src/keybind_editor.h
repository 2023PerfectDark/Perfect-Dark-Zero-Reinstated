#pragma once
#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/keybinds.h>
#include "utils.h"
#include "wheel_bindings.h"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
namespace pdz_bindings {
static rex::ui::VirtualKey ImGuiKeyToVirtualKey(ImGuiKey key) {
  using VK = rex::ui::VirtualKey;
  if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
    return static_cast<VK>(static_cast<uint16_t>(VK::kA) + (key - ImGuiKey_A));
  }
  if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
    return static_cast<VK>(static_cast<uint16_t>(VK::k0) + (key - ImGuiKey_0));
  }
  if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) {
    return static_cast<VK>(static_cast<uint16_t>(VK::kF1) + (key - ImGuiKey_F1));
  }
  if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) {
    return static_cast<VK>(static_cast<uint16_t>(VK::kNumpad0) + (key - ImGuiKey_Keypad0));
  }
  switch (key) {
    case ImGuiKey_Space:
      return VK::kSpace;
    case ImGuiKey_Enter:
      return VK::kReturn;
    case ImGuiKey_Escape:
      return VK::kEscape;
    case ImGuiKey_Tab:
      return VK::kTab;
    case ImGuiKey_Backspace:
      return VK::kBack;
    case ImGuiKey_Delete:
      return VK::kDelete;
    case ImGuiKey_Insert:
      return VK::kInsert;
    case ImGuiKey_Home:
      return VK::kHome;
    case ImGuiKey_End:
      return VK::kEnd;
    case ImGuiKey_PageUp:
      return VK::kPrior;
    case ImGuiKey_PageDown:
      return VK::kNext;
    case ImGuiKey_LeftArrow:
      return VK::kLeft;
    case ImGuiKey_RightArrow:
      return VK::kRight;
    case ImGuiKey_UpArrow:
      return VK::kUp;
    case ImGuiKey_DownArrow:
      return VK::kDown;
    case ImGuiKey_LeftShift:
    case ImGuiKey_RightShift:
      return VK::kShift;
    case ImGuiKey_LeftCtrl:
    case ImGuiKey_RightCtrl:
      return VK::kControl;
    case ImGuiKey_LeftAlt:
    case ImGuiKey_RightAlt:
      return VK::kMenu;
    case ImGuiKey_CapsLock:
      return VK::kCapital;
    case ImGuiKey_NumLock:
      return VK::kNumLock;
    case ImGuiKey_ScrollLock:
      return VK::kScroll;
    case ImGuiKey_PrintScreen:
      return VK::kSnapshot;
    case ImGuiKey_Pause:
      return VK::kPause;
    case ImGuiKey_GraveAccent:
      return VK::kOem3;
    case ImGuiKey_Minus:
      return VK::kOemMinus;
    case ImGuiKey_Equal:
      return VK::kOemPlus;
    case ImGuiKey_LeftBracket:
      return VK::kOem4;
    case ImGuiKey_RightBracket:
      return VK::kOem6;
    case ImGuiKey_Backslash:
      return VK::kOem5;
    case ImGuiKey_Semicolon:
      return VK::kOem1;
    case ImGuiKey_Apostrophe:
      return VK::kOem7;
    case ImGuiKey_Comma:
      return VK::kOemComma;
    case ImGuiKey_Period:
      return VK::kOemPeriod;
    case ImGuiKey_Slash:
      return VK::kOem2;
    case ImGuiKey_KeypadDecimal:
      return VK::kDecimal;
    case ImGuiKey_KeypadDivide:
      return VK::kDivide;
    case ImGuiKey_KeypadMultiply:
      return VK::kMultiply;
    case ImGuiKey_KeypadSubtract:
      return VK::kSubtract;
    case ImGuiKey_KeypadAdd:
      return VK::kAdd;
    case ImGuiKey_KeypadEnter:
      return VK::kReturn;
    default:
      return VK::kNone;
  }
}
inline std::vector<std::string> Names(){
 auto names=rex::cvar::ListFlagsByCategory("Input/Keybinds/Controller");
 if(rex::cvar::GetFlagInfo("bind_third_person"))names.push_back("bind_third_person");
 return names;
}
inline void Persist(bool& saved){
 std::ofstream file(utils::SettingsDir()/"keybindings.toml",std::ios::trunc);
 for(const auto& name:Names())file<<name<<" = "<<std::quoted(rex::cvar::GetFlagByName(name))<<"\n";file.close();
 saved=bool(file);
}
inline void Draw(bool& saved){
 static float editorScale=2.1f;
 static bool scalingGesture=false;
 const bool ctrl=ImGui::GetIO().KeyCtrl || ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
 if(ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&ctrl&&ImGui::GetIO().MouseWheel!=0){
  scalingGesture=true;
  editorScale=std::clamp(editorScale+ImGui::GetIO().MouseWheel*.15f,1.2f,3.5f);
  ImGui::GetIO().MouseWheel=0;
 }
 if(ImGui::Button("Open mouse and keyboard editor"))ImGui::OpenPopup("Mouse and keyboard controls");
 const auto size=ImGui::GetMainViewport()->Size;
 ImGui::SetNextWindowSize({size.x*.78f,size.y*.78f},ImGuiCond_Appearing);
 if(!ImGui::BeginPopupModal("Mouse and keyboard controls",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
 ImGui::SetWindowFontScale(editorScale);
 ImGui::SliderFloat("Text size",&editorScale,1.2f,3.5f,"%.2fx");
 ImGui::BeginChild("binding-list",{0,-50},false);
 ImGui::SetWindowFontScale(editorScale);
 static std::string selected,candidate,conflicts;static int opened=0;
 ImGui::Separator();ImGui::TextUnformatted("Mouse and keyboard bindings");
 ImGui::TextWrapped("Select an action, then press a key or mouse button. Delete clears it. Escape offers Bind or Cancel. Hold Ctrl and scroll to resize text. Scroll up/down to bind button actions. While aiming, wheel only zooms.");
 for(const auto& name:Names()){
  const auto* info=rex::cvar::GetFlagInfo(name);if(!info)continue;
  ImGui::PushID(name.c_str());
  const auto value=rex::cvar::GetFlagByName(name);
  std::string label=info->description+": "+(value.empty()?"Unbound":value);
  if(ImGui::Selectable(label.c_str())){selected=name;candidate.clear();conflicts.clear();opened=ImGui::GetFrameCount();ImGui::OpenPopup("Assign binding");}
  if(ImGui::BeginPopupModal("Assign binding",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
   ImGui::SetWindowFontScale(editorScale);
   ImGui::TextUnformatted(info->description.c_str());
   ImGui::TextUnformatted(selected=="bind_third_person" ? "Press a camera toggle key. Delete: clear. Escape: bind or cancel." : "Press a key/button or scroll. Delete: clear. Escape: bind or cancel.");
   if(ImGui::GetFrameCount()>opened+1 && candidate.empty()){
    if(ImGui::IsKeyPressed(ImGuiKey_Delete)){rex::cvar::SetFlagByName(selected,"");Persist(saved);ImGui::CloseCurrentPopup();}
    else {
     for(int k=ImGuiKey_NamedKey_BEGIN;k<ImGuiKey_GamepadStart;++k){
      auto key=static_cast<ImGuiKey>(k);
      const bool modifier=key==ImGuiKey_LeftShift||key==ImGuiKey_RightShift||key==ImGuiKey_LeftCtrl||key==ImGuiKey_RightCtrl||key==ImGuiKey_LeftAlt||key==ImGuiKey_RightAlt;
      if(scalingGesture && (key==ImGuiKey_LeftCtrl||key==ImGuiKey_RightCtrl))continue;
      if(modifier ? !ImGui::IsKeyReleased(key) : !ImGui::IsKeyPressed(key,false))continue;
      auto vk=ImGuiKeyToVirtualKey(key);if(vk==rex::ui::VirtualKey::kNone)continue;
      candidate=rex::ui::VirtualKeyToString(vk);
      const auto& io=ImGui::GetIO();
      if(selected!="bind_third_person" && io.KeyShift && vk!=rex::ui::VirtualKey::kShift)candidate="Shift+"+candidate;
      if(selected!="bind_third_person" && io.KeyCtrl && vk!=rex::ui::VirtualKey::kControl)candidate="Ctrl+"+candidate;
      if(selected!="bind_third_person" && io.KeyAlt && vk!=rex::ui::VirtualKey::kMenu)candidate="Alt+"+candidate;
      break;
     }
     for(int b=0;selected!="bind_third_person" && b<3 && candidate.empty();++b)if(ImGui::IsMouseClicked(b))candidate=std::string(b==0?"LMB":b==1?"RMB":b==2?"MMB":b==3?"X1":"X2");
     if(candidate.empty() && pdz_input::WheelAssignable(selected) && ImGui::GetIO().MouseWheel!=0)candidate=ImGui::GetIO().MouseWheel>0?"WheelUp":"WheelDown";
     if(!candidate.empty())for(const auto& other:rex::cvar::ListFlags()){
      if(other==selected || !(other.starts_with("keybind_")||other.starts_with("bind_")))continue;
      std::istringstream tokens(rex::cvar::GetFlagByName(other));std::string token;
      while(std::getline(tokens,token,','))if(token==candidate){auto* entry=rex::cvar::GetFlagInfo(other);conflicts+=(entry?entry->description:other)+"\n";break;}
     }
    }
   }
   if(!candidate.empty()){
    ImGui::Text("New binding: %s",candidate.c_str());
    if(candidate=="Escape")ImGui::TextUnformatted("Bind Escape to this action, or cancel?");
    if(!conflicts.empty()){ImGui::TextUnformatted("Already assigned to:");ImGui::TextUnformatted(conflicts.c_str());}
    if(ImGui::Button(candidate=="Escape"?"Bind Escape":"Assign")){rex::cvar::SetFlagByName(selected,candidate);Persist(saved);ImGui::CloseCurrentPopup();}
    ImGui::SameLine();if(ImGui::Button("Try another key")){candidate.clear();conflicts.clear();opened=ImGui::GetFrameCount();}
   }
   if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
   ImGui::EndPopup();
  }
  ImGui::PopID();
 }
 if(ImGui::Button("Reset bindings to defaults"))ImGui::OpenPopup("Reset bindings?");
 if(ImGui::BeginPopupModal("Reset bindings?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::SetWindowFontScale(editorScale);
  ImGui::TextUnformatted("Reset all mouse and keyboard controller bindings?\nYour current assignments will be replaced.");
  if(ImGui::Button("Reset")){for(const auto& name:Names())rex::cvar::ResetToDefault(name);Persist(saved);ImGui::CloseCurrentPopup();}
  ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
 }
 ImGui::EndChild();
 if(!ctrl)scalingGesture=false;
 if(ImGui::Button("Close editor"))ImGui::CloseCurrentPopup();
 ImGui::EndPopup();
}
}
