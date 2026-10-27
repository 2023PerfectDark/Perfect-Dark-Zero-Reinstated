#pragma once
#include <cstdint>
#include <string_view>
namespace pdz_input {
struct WheelButton {const char* name;uint16_t mask;};
inline constexpr WheelButton wheel_buttons[]={
 {"keybind_a",0x1000},{"keybind_b",0x2000},{"keybind_x",0x4000},{"keybind_y",0x8000},
 {"keybind_left_shoulder",0x100},{"keybind_right_shoulder",0x200},
 {"keybind_lstick_press",0x40},{"keybind_rstick_press",0x80},
 {"keybind_dpad_up",1},{"keybind_dpad_down",2},{"keybind_dpad_left",4},{"keybind_dpad_right",8},
 {"keybind_back",0x20},{"keybind_start",0x10}};
inline bool WheelAssignable(std::string_view name){for(auto b:wheel_buttons)if(name==b.name)return true;return false;}
inline bool HasWheel(std::string_view list,std::string_view wheel){
 while(!list.empty()){auto end=list.find(',');auto token=list.substr(0,end);
 while(!token.empty()&&token.front()==' ')token.remove_prefix(1);
 while(!token.empty()&&token.back()==' ')token.remove_suffix(1);
 if(token==wheel)return true;if(end==std::string_view::npos)break;list.remove_prefix(end+1);}
 return false;
}
inline bool SuppressWheelActions(bool mouseAim,unsigned trigger){return mouseAim||trigger>0;}
}
