#pragma once
#include "pd_menu_font.h"
#include "game_cvars.h"
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstdlib>

namespace pdz_menu {
// Host rendering of the original menuitem.c layout: 10px label inset,
// right-aligned 6px checks, text-only selectable rows and twin-line cursor.
class Classic {
  ImGuiID dragging_slider_=0;
 public:
  BitmapFont font{"handelgothicsm.bin"};
  float scale=1;
  std::string selected_help;
  void Section(const char* name){ImGui::Separator();Text(name,IM_COL32(255,210,80,255));}
  void Text(std::string_view value,ImU32 colour=IM_COL32(0,255,255,255)) {
    auto p=ImGui::GetCursorScreenPos();
    font.Text(ImGui::GetWindowDrawList(),p,scale,colour,value);
    ImGui::Dummy({font.Text(nullptr,p,scale,colour,value),13*scale});
  }
  void Wrapped(std::string_view value,ImU32 colour=IM_COL32(0,255,255,255)) {
    std::string line;
    const float width=ImGui::GetContentRegionAvail().x;
    size_t start=0;
    while(start<value.size()) {
      auto end=value.find_first_of(" \n",start);
      if(end==std::string_view::npos)end=value.size();
      std::string word(value.substr(start,end-start));
      if(!line.empty()&&font.Text(nullptr,{},scale,0,line+" "+word)>width){Text(line,colour);line.clear();}
      if(!line.empty())line+=' ';
      line+=word;
      if(end<value.size()&&value[end]=='\n'){Text(line,colour);line.clear();}
      start=end+1;
    }
    if(!line.empty())Text(line,colour);
  }
  bool Row(const char* label,const char* value=nullptr,int checked=-1) {
    auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Header,0);ImGui::PushStyleColor(ImGuiCol_HeaderHovered,0);ImGui::PushStyleColor(ImGuiCol_HeaderActive,0);
    bool activated=ImGui::Selectable("##row",false,0,{width,13*scale});
    bool focused=ImGui::IsItemFocused()||ImGui::IsItemHovered();
    if(ImGui::IsItemFocused()&&!ImGui::IsItemVisible())ImGui::SetScrollHereY();
    ImGui::PopStyleColor(3);ImGui::PopID();
    auto* d=ImGui::GetWindowDrawList();
    ImU32 colour=focused?IM_COL32(190,255,255,255):IM_COL32(0,255,255,255);
    if(focused){
      selected_help=label;
      float pulse=.5f+.5f*std::sin(float(ImGui::GetTime())*6.283185f/0.666667f);
      colour=IM_COL32(int(150+105*pulse),255,255,255);
      for(int i=0;i<2;++i)d->AddRectFilledMultiColor({p.x,p.y+(4+i*2)*scale},{p.x+8*scale,p.y+(5+i*2)*scale},IM_COL32(255,255,255,0),colour,colour,IM_COL32(255,255,255,0));
    }
    font.Text(d,{p.x+10*scale,p.y+2*scale},scale,colour,label);
    if(value){float w=font.Text(nullptr,{},scale,colour,value);font.Text(d,{p.x+width-w-10*scale,p.y+2*scale},scale,colour,value);}
    if(checked>=0){ImVec2 a{p.x+width-16*scale,p.y+2*scale},b{a.x+6*scale,a.y+6*scale};d->AddRect(a,b,colour,0,0,scale);if(checked)d->AddRectFilled({a.x+scale,a.y+scale},{b.x-scale,b.y-scale},colour);}
    return activated;
  }
  bool Toggle(const char* label,bool& value){if(Row(label,nullptr,value)){value=!value;return true;}return false;}
  bool Choice(const char* label,int& value,const char* const* choices,int count) {
    ImGui::PushID(label);
    auto pos=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    if(Row(label,choices[value]))ImGui::OpenPopup("choices");
    float popup_width=80*scale;
    for(int i=0;i<count;++i)popup_width=std::max(popup_width,font.Text(nullptr,{},scale,0,choices[i])+22*scale);
    ImGui::SetNextWindowPos({pos.x+width-popup_width,pos.y+13*scale});
    ImGui::SetNextWindowSize({popup_width,0});
    ImGui::PushStyleColor(ImGuiCol_PopupBg,IM_COL32(0,0,90,240));
    bool changed=false;
    if(ImGui::BeginPopup("choices")) {
      for(int i=0;i<count;++i){
        if(ImGui::IsWindowAppearing()&&i==value)ImGui::SetKeyboardFocusHere();
        if(Row(choices[i])){value=i;changed=true;ImGui::CloseCurrentPopup();}
      }
      ImGui::EndPopup();
    }
    ImGui::PopStyleColor();ImGui::PopID();return changed;
  }
  bool Number(const char* label,float& value,float lo,float hi,float step) {
    auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    const ImGuiID id=ImGui::GetID(label);
    bool clicked=Row(label),active=ImGui::IsItemFocused();
    const bool hovered=ImGui::IsItemHovered();
    float left=p.x+width-110*scale,right=p.x+width-35*scale,y=p.y+11*scale;
    const float old=value;
    const auto& io=ImGui::GetIO();
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))dragging_slider_=0;
    if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&io.MousePos.x>=left&&io.MousePos.x<=right)
      dragging_slider_=id;
    if(dragging_slider_==id){
      float target=lo+(hi-lo)*std::clamp((io.MousePos.x-left)/(right-left),0.f,1.f);
      value=std::clamp(lo+std::round((target-lo)/step)*step,lo,hi);
    }else if(active){
      int direction=0;
      if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)||ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft))direction=-1;
      if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight))direction=1;
      // Mouse release must not add a step after finishing a drag.
      if(clicked&&!ImGui::IsMouseReleased(ImGuiMouseButton_Left))direction=1;
      value=std::clamp(value+direction*step,lo,hi);
    }
    auto* d=ImGui::GetWindowDrawList();
    float marker=left+(right-left)*std::clamp((value-lo)/(hi-lo),0.f,1.f);
    // menugfx_render_slider: blue ramp, downward marker and blue/white rail.
    d->AddTriangleFilled({left,y},{right-4*scale,y-6*scale},{right,y},IM_COL32(0,0,255,79));
    d->AddTriangleFilled({marker-4*scale,y-8*scale},{marker+4*scale,y-8*scale},{marker,y+scale},IM_COL32(200,255,255,180));
    d->AddRectFilledMultiColor({left,y},{marker,y+scale},IM_COL32(0,0,255,255),IM_COL32_WHITE,IM_COL32_WHITE,IM_COL32(0,0,255,255));
    d->AddRectFilled({marker,y},{right,y+scale},IM_COL32(0,0,255,255));
    char number[32];
    std::snprintf(number,sizeof(number),step<1.f?"%.2f":"%.0f",double(value));
    float number_width=font.Text(nullptr,{},scale,0,number);
    font.Text(d,{p.x+width-7*scale-number_width,p.y+2*scale},scale,IM_COL32(190,255,255,255),number);
    return value!=old;
  }
  void Settings(bool& saved,int tab) {
#define PD_BOOL(label,name) {bool v=REXCVAR_GET(name);if(Toggle(label,v)){REXCVAR_SET(name,v);saved=false;}}
#define PD_FLOAT(label,name,lo,hi,step) {float v=float(REXCVAR_GET(name));if(Number(label,v,lo,hi,step)){REXCVAR_SET(name,double(v));saved=false;}}
#define PD_INT(label,name,lo,hi,step) {float v=float(REXCVAR_GET(name));if(Number(label,v,lo,hi,step)){REXCVAR_SET(name,int(v));saved=false;}}
    if(tab==0){
      Section("Mouse look");
      PD_BOOL("Direct mouse look",pdz_mousehook);
      PD_FLOAT("Sensitivity",pdz_mouse_sensitivity,.05f,10.f,.05f);
      PD_BOOL("Invert X",pdz_mouse_invert_x);
      PD_BOOL("Invert Y",pdz_mouse_invert_y);
      Section("Menus and aiming");
      PD_BOOL("Mouse menu navigation",pdz_mouse_menu_navigation);
      PD_BOOL("Mouse wheel aim zoom",pdz_wheel_ads);
      PD_INT("Aim trigger pressure",pdz_ads_pressure,32,255,1);
    }else if(tab==1){
      Section("Quality preset - save and restart");
      const auto getInt=[](const char* name){return std::atoi(rex::cvar::GetFlagByName(name).c_str());};
      const int scales[]={1,1,2,3},filters[]={2,3,4,5};
      const bool antialias[]={false,true,true,true};
      int preset=4;
      for(int i=0;i<4;++i)if(getInt("resolution_scale")==scales[i]&&getInt("anisotropic_override")==filters[i]&&
          (rex::cvar::GetFlagByName("native_2x_msaa")=="true")==antialias[i])preset=i;
      const char* names[]={"Low","Medium","High","Ultra","Custom"};
      if(Choice("Graphics preset",preset,names,5)&&preset<4){
        rex::cvar::SetFlagByName("resolution_scale",std::to_string(scales[preset]));
        rex::cvar::SetFlagByName("anisotropic_override",std::to_string(filters[preset]));
        rex::cvar::SetFlagByName("native_2x_msaa",antialias[preset]?"true":"false");saved=false;
      }
      Section("Image tone - save and restart");
      const auto number=[&](const char* label,const char* name,float lo,float hi,float step){
        const auto text=rex::cvar::GetFlagByName(name);if(text.empty())return;
        float v=std::strtof(text.c_str(),nullptr);
        if(Number(label,v,lo,hi,step)&&rex::cvar::SetFlagByName(name,step>=1?std::to_string(int(v)):std::to_string(v)))saved=false;
      };
      number("Shadow lift","pdz_shadow_lift",0,.5f,.01f);
      Section("Performance-intensive options");
      Wrapped("Higher quality increases GPU load. Expect lower FPS on GPU-limited systems.",IM_COL32(255,170,70,255));
      PD_FLOAT("Field of view",pdz_fov,40.f,120.f,1.f);
      PD_BOOL("First-person body",pdz_first_person_body);
      PD_BOOL("Visible holstered weapons",pdz_visible_holsters);
      PD_BOOL("Third-person camera",pdz_tp_enable);
      const char* filtering[]={"Game default","Off","1x","2x","4x","8x","16x"};
      int af=std::clamp(getInt("anisotropic_override")+1,0,6);
      if(Choice("Texture filtering",af,filtering,7)){rex::cvar::SetFlagByName("anisotropic_override",std::to_string(af-1));saved=false;}
      Text("Below: save and restart",IM_COL32(255,210,80,255));
      bool msaa=rex::cvar::GetFlagByName("native_2x_msaa")=="true";
      if(Toggle("Native 2x anti-aliasing",msaa)){rex::cvar::SetFlagByName("native_2x_msaa",msaa?"true":"false");saved=false;}
      number("Resolution scale","resolution_scale",1,4,1);
    }else if(tab==2){
      Section("General");
      static const char* languages[]={"English","Deutsch","Japanese","Spanish (LA)","Spanish (EU)","French","Italian"};
      int language=std::clamp(int(REXCVAR_GET(pdz_dub_language)),0,6);
      if(Choice("Voice language",language,languages,7))REXCVAR_SET(pdz_dub_language,language);
      PD_BOOL("Skip intro (next launch)",pdz_skip_intro);
      Section("Weapons");
      PD_BOOL("All weapon aim poses",pdz_all_weapon_ads);
      PD_BOOL("Automatic fire while zoomed",pdz_campaign_zoom_auto);
      Section("Performance-intensive effects");
      Wrapped("Gore and longer decal lifetimes can reduce FPS and increase memory use.",IM_COL32(255,170,70,255));
      PD_BOOL("Sniper headshot gore",pdz_campaign_gore);
      PD_INT("Gore lifetime (minutes)",pdz_gore_decal_minutes,1,20,1);
    }else if(tab==3){
      Section("Player cheats");
      PD_BOOL("Invisibility",pdz_invisibility);
      PD_BOOL("Invincibility",pdz_invincibility);
      Section("Gameplay assists");
      PD_BOOL("Training ammo refill",pdz_training_ammo_refill);
      PD_BOOL("Remove aiming spread",pdz_campaign_no_spread);
      PD_BOOL("Ignore weapon size",pdz_ignore_weapon_size);
    }
    if(tab==6){
      Section("Display - save and restart");
      const char* modes[]={"Windowed","Borderless","Exclusive"};
      int mode=rex::cvar::GetFlagByName("fullscreen")!="true"?0:rex::cvar::GetFlagByName("fullscreen_exclusive")=="true"?2:1;
      if(Choice("Display mode",mode,modes,3)){
        rex::cvar::SetFlagByName("fullscreen",mode?"true":"false");
        rex::cvar::SetFlagByName("fullscreen_exclusive",mode==2?"true":"false");saved=false;
      }
      bool sync=rex::cvar::GetFlagByName("vsync")=="true";
      if(Toggle("VSync",sync)){rex::cvar::SetFlagByName("vsync",sync?"true":"false");saved=false;}
      bool bars=rex::cvar::GetFlagByName("present_letterbox")=="true";
      if(Toggle("Preserve aspect ratio",bars)){rex::cvar::SetFlagByName("present_letterbox",bars?"true":"false");saved=false;}
      Section("Cutscene pacing");
      PD_BOOL("Cutscene 60 FPS limit",pdz_cutscene_60fps);
      Section("Higher performance demand");
      Wrapped("A higher frame-rate target increases CPU and GPU demand. Sustained FPS depends on your hardware.",IM_COL32(255,170,70,255));
      PD_BOOL("60 FPS unlock",pdz_fps60_unlock);
    }
    if(tab==5){
      Section("Screenshots");
      PD_BOOL("Hide HUD",pdz_hide_hud);
      Section("PD XBLA menu");
      PD_FLOAT("UI size",pdz_ui_scale,.65f,1.4f,.05f);
      PD_FLOAT("UI width",pdz_ui_width,.85f,1.4f,.05f);
      if(Row("Reset UI layout")){REXCVAR_SET(pdz_ui_scale,1.0);REXCVAR_SET(pdz_ui_width,1.0);saved=false;}
    }
    ImGui::Separator();
    if(Row("Save settings")){rex::cvar::SaveConfig(utils::SettingsDir()/"hardware.toml");saved=true;}
    if(saved)Text("Settings saved.");
#undef PD_BOOL
#undef PD_FLOAT
#undef PD_INT
  }
};
}
