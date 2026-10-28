#pragma once
#include "pd_menu_font.h"
#include "game_cvars.h"
#include "debug_position.h"
#include "keybind_editor.h"
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstdlib>

namespace pdz_menu {
// Host rendering of the original menuitem.c layout: 10px label inset,
// right-aligned 6px checks, text-only selectable rows and twin-line cursor.
class Classic {
  ImGuiID dragging_slider_=0;
  float drag_left_=0,drag_right_=1;
 public:
  BitmapFont font{"handelgothicsm.bin"};
  float scale=1;
  std::string selected_help;
  bool hover_selected=false;
  void Section(const char* name){ImGui::Separator();Text(name,IM_COL32(255,210,80,255));}
  void Text(std::string_view value,ImU32 colour=IM_COL32(0,255,255,255)) {
    auto p=ImGui::GetCursorScreenPos();
    font.Text(ImGui::GetWindowDrawList(),p,scale,colour,value);
    ImGui::Dummy({font.Text(nullptr,p,scale,colour,value),13*scale});
  }
  int WrappedLines(std::string_view value,float width,float text_scale) const {
    int count=0;std::string line;size_t start=0;
    while(start<value.size()){
      auto end=value.find_first_of(" \n",start);
      if(end==std::string_view::npos)end=value.size();
      std::string word(value.substr(start,end-start));
      if(!line.empty()&&font.Text(nullptr,{},text_scale,0,line+" "+word)>width){++count;line.clear();}
      if(!line.empty())line+=' ';
      line+=word;
      if(end<value.size()&&value[end]=='\n'){++count;line.clear();}
      start=end+1;
    }
    return count+(!line.empty()?1:0);
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
    // ImGui scrolls a newly navigated item into view itself. Re-centering a
    // retained focus every frame fights mouse-wheel scrolling through lists.
    ImGui::PopStyleColor(3);ImGui::PopID();
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,{p.x+width,p.y+13*scale},IM_COL32(0,55,105,65));
    d->AddLine({p.x,p.y+13*scale},{p.x+width,p.y+13*scale},IM_COL32(30,150,210,70),scale*.35f);
    ImU32 colour=focused?IM_COL32(190,255,255,255):IM_COL32(0,255,255,255);
    if(focused){
      if(ImGui::IsItemHovered()||!hover_selected)selected_help=label;
      if(ImGui::IsItemHovered())hover_selected=true;
      const float glow=.5f+.5f*std::sin(float(ImGui::GetTime())*3.f);
      d->AddRectFilledMultiColor(p,{p.x+width,p.y+13*scale},IM_COL32(0,140,220,int(20+12*glow)),IM_COL32(0,90,180,4),IM_COL32(0,90,180,4),IM_COL32(0,140,220,int(20+12*glow)));
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
      {dragging_slider_=id;drag_left_=left;drag_right_=right;}
    if(dragging_slider_==id){
      float target=lo+(hi-lo)*std::clamp((io.MousePos.x-drag_left_)/(drag_right_-drag_left_),0.f,1.f);
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
  void Settings(bool& saved,int tab,bool show_debug=true) {
#define PD_BOOL(label,name) {bool v=REXCVAR_GET(name);if(Toggle(label,v)){REXCVAR_SET(name,v);saved=false;}}
#define PD_FLOAT(label,name,lo,hi,step) {float v=float(REXCVAR_GET(name));if(Number(label,v,lo,hi,step)){REXCVAR_SET(name,double(v));saved=false;}}
#define PD_INT(label,name,lo,hi,step) {float v=float(REXCVAR_GET(name));if(Number(label,v,lo,hi,step)){REXCVAR_SET(name,int(v));saved=false;}}
    if(tab==0){
      Section("Mouse look");
      PD_BOOL("Direct mouse look",pdz_mousehook);
      PD_FLOAT("Sensitivity",pdz_mouse_sensitivity,.05f,10.f,.05f);
      PD_BOOL("Invert X",pdz_mouse_invert_x);
      PD_BOOL("Invert Y",pdz_mouse_invert_y);
      Section("Walking");
      PD_FLOAT("Walking speed (%)",pdz_walk_speed,10.f,100.f,5.f);
      Section("Menus and aiming");
      PD_BOOL("Mouse menu navigation",pdz_mouse_menu_navigation);
      pdz_bindings::Draw(saved,Row("Mouse and keyboard bindings"));
      PD_INT("Aim trigger pressure",pdz_ads_pressure,32,255,1);
    }else if(tab==1){
      Section("Graphics API");
      const char* renderers[]={"Direct3D 12","Vulkan"};
      int renderer=rex::cvar::GetFlagByName("graphics_backend")=="vulkan"?1:0;
      if(Choice("Renderer",renderer,renderers,2)){
        rex::cvar::SetFlagByName("graphics_backend",renderer?"vulkan":"d3d12");
        saved=false;
      }
      if(renderer==0){
        const char* paths[]={"Automatic","ROV","RTV"};
        const auto path=rex::cvar::GetFlagByName("render_target_path_d3d12");
        int selected=path=="rov"?1:path=="rtv"?2:0;
        if(Choice("Render target mode",selected,paths,3)){
          rex::cvar::SetFlagByName("render_target_path_d3d12",selected==1?"rov":selected==2?"rtv":"");saved=false;
        }
      }
      Section("Shader loading");
      bool playThrough=rex::cvar::GetFlagByName("async_shader_compilation")=="true" &&
          !REXCVAR_GET(pdz_pause_for_shaders) &&
          (renderer==0 || rex::cvar::GetFlagByName("vulkan_async_skip_incomplete_frames")!="true");
      if(Toggle("Play while shaders compile",playThrough)){
        rex::cvar::SetFlagByName("async_shader_compilation","true");
        if(playThrough) rex::cvar::SetFlagByName("async_pipeline_wait_ms","0");
        rex::cvar::SetFlagByName("pdz_pause_for_shaders",playThrough?"false":"true");
        rex::cvar::SetFlagByName("vulkan_async_skip_incomplete_frames","false");
        saved=false;
      }
      Section("Quality preset");
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
      const auto number=[&](const char* label,const char* name,float lo,float hi,float step){
        const auto text=rex::cvar::GetFlagByName(name);if(text.empty())return;
        float v=std::strtof(text.c_str(),nullptr);
        if(Number(label,v,lo,hi,step)&&rex::cvar::SetFlagByName(name,step>=1?std::to_string(int(v)):std::to_string(v)))saved=false;
      };
      if(!rex::cvar::GetFlagByName("pdz_shadow_lift").empty()){
        Section("Image tone");
        number("Shadow lift","pdz_shadow_lift",0,.5f,.01f);
      }
      Section("Camera and character");
      PD_FLOAT("Field of view",pdz_fov,40.f,120.f,1.f);
      PD_BOOL("First-person body",pdz_first_person_body);
      PD_BOOL("Visible holstered weapons",pdz_visible_holsters);
      PD_BOOL("Third-person camera",pdz_tp_enable);
      bool headMotion=REXCVAR_GET(head_roll)!=0;
      if(Toggle("Breathing sway / head roll",headMotion)){REXCVAR_SET(head_roll,headMotion?1:0);saved=false;}
      Section("Image quality");
      const char* filtering[]={"Game default","Off","1x","2x","4x","8x","16x"};
      int af=std::clamp(getInt("anisotropic_override")+1,0,6);
      if(Choice("Texture filtering",af,filtering,7)){rex::cvar::SetFlagByName("anisotropic_override",std::to_string(af-1));saved=false;}
      bool msaa=rex::cvar::GetFlagByName("native_2x_msaa")=="true";
      if(Toggle("Native 2x anti-aliasing",msaa)){rex::cvar::SetFlagByName("native_2x_msaa",msaa?"true":"false");saved=false;}
      number("Resolution scale","resolution_scale",1,4,1);
    }else if(tab==2){
      Section("General");
      static const char* languages[]={"English","Deutsch","Japanese","Spanish (LA)","Spanish (EU)","French","Italian"};
      int language=std::clamp(int(REXCVAR_GET(pdz_dub_language)),0,6);
      if(Choice("Voice language",language,languages,7)){REXCVAR_SET(pdz_dub_language,language);saved=false;}
      static const char* displayNames[]={"English","Japanese","German","French","Spanish (EU)","Italian","Korean","Chinese","Spanish (LA)"};
      static const int displayIds[]={1,2,3,4,5,6,7,8,10};
      int display=0;for(int n=0;n<9;++n)if(std::to_string(displayIds[n])==rex::cvar::GetFlagByName("user_language"))display=n;
      if(Choice("Display language",display,displayNames,9)){rex::cvar::SetFlagByName("user_language",std::to_string(displayIds[display]));saved=false;}
      PD_BOOL("Skip intro (next launch)",pdz_skip_intro);
      PD_BOOL("Mission timer",pdz_mission_timer);
      PD_BOOL("Cutscene subtitles",pdz_cutscene_subtitles);
      Section("Weapons");
      PD_BOOL("All weapon aim poses",pdz_all_weapon_ads);
      PD_BOOL("Automatic fire while zoomed",pdz_campaign_zoom_auto);
      Section("Performance-intensive effects");
      PD_BOOL("Sniper headshot gore",pdz_campaign_gore);
      PD_INT("Gore lifetime (minutes)",pdz_gore_decal_minutes,1,20,1);
    }else if(tab==3){
      if(show_debug){
        Section("Debug");
        if(Row("Debug Position",nullptr,pdz_position::Enabled()))pdz_position::Toggle();
      }
        Section("Player cheats");
      PD_BOOL("Invisibility",pdz_invisibility);
      PD_BOOL("Invincibility",pdz_invincibility);
      Section("Gameplay assists");
      PD_BOOL("Backpack reload",pdz_backpack_reload);
      PD_BOOL("Training ammo refill",pdz_training_ammo_refill);
      PD_BOOL("Ignore weapon size",pdz_ignore_weapon_size);
    }
    if(tab==6){
      Section("Display");
      const char* modes[]={"Windowed","Borderless","Exclusive"};
      int mode=rex::cvar::GetFlagByName("fullscreen")!="true"?0:rex::cvar::GetFlagByName("fullscreen_exclusive")=="true"?2:1;
      if(Choice("Display mode",mode,modes,3)){
        rex::cvar::SetFlagByName("fullscreen",mode?"true":"false");
        rex::cvar::SetFlagByName("fullscreen_exclusive",mode==2?"true":"false");saved=false;
      }
      const char* resolutions[]={"Automatic","240p (426 x 240)","360p (640 x 360)","480p (854 x 480)","720p (1280 x 720)","900p (1600 x 900)","1080p (1920 x 1080)","1440p (2560 x 1440)","4K (3840 x 2160)"};
      const char* modesizes[]={"","426x240","640x360","854x480","1280x720","1600x900","1920x1080","2560x1440","3840x2160"};
      int output=0;const auto configured=rex::cvar::GetFlagByName("resolution");
      for(int i=1;i<9;++i)if(configured==modesizes[i])output=i;
      if(Choice("Output resolution",output,resolutions,9)){
        rex::cvar::SetFlagByName("window_width","0");
        rex::cvar::SetFlagByName("window_height","0");
        rex::cvar::SetFlagByName("resolution",modesizes[output]);saved=false;
      }
      Section("Presentation");
      bool sync=rex::cvar::GetFlagByName("vsync")=="true";
      if(Toggle("VSync",sync)){rex::cvar::SetFlagByName("vsync",sync?"true":"false");saved=false;}
      bool bars=rex::cvar::GetFlagByName("present_letterbox")=="true";
      if(Toggle("Preserve aspect ratio",bars)){rex::cvar::SetFlagByName("present_letterbox",bars?"true":"false");saved=false;}
      Section("Frame pacing");
      PD_BOOL("Menu / scripted camera cap (60 FPS)",pdz_cutscene_60fps);
    }
    if(tab==5){
      Section("Screenshots");
      PD_BOOL("Hide HUD",pdz_hide_hud);
      Section("PD XBLA menu");
      PD_FLOAT("UI size",pdz_ui_scale,.65f,1.4f,.01f);
      PD_FLOAT("UI width",pdz_ui_width,.85f,1.4f,.01f);
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

