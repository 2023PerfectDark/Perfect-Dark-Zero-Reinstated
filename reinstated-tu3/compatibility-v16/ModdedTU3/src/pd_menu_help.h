#pragma once
#include <string_view>
namespace pdz_menu {
inline std::string_view HelpFor(std::string_view label){
  struct Entry{std::string_view label,text;};
  static constexpr Entry entries[]={
    {"Renderer","Choose Direct3D 12 or Vulkan. Save settings and restart to switch."},
    {"Render target mode","Choose Automatic, ROV or RTV for Direct3D 12. Unsupported ROV falls back to RTV. Applies after pending GPU work finishes. Save settings to keep your choice."},
    {"Play while shaders compile","On: keep playing during compilation; objects may pop in or briefly appear purple. Off: wait at loading and pause for new shaders. Applies now; save to keep it."},
    {"Walking speed (%)","Hold Left Shift with movement keys to walk at this percentage of normal speed."},
    {"Mouse and keyboard bindings","Assign keys and mouse buttons. Right mouse plus wheel remains aim zoom. Delete clears a binding; reset asks for confirmation."},
    {"Output resolution","Choose 240p to 4K output for windowed or exclusive mode. Borderless uses the desktop size. Applies immediately; save settings to keep your choice."},
    {"Display language","Choose game text language. Requires matching installed files. The game reads this setting when loading localized text."},
    {"Mission timer","Show elapsed mission time, including milliseconds."},
    {"Cutscene subtitles","Display spoken cutscene dialogue as subtitles."},
    {"Breathing sway / head roll","Enable the character's breathing sway and head-roll motion."},
    {"Graphics preset","Low: 1x rendering, 2x filtering, MSAA off. Medium: 1x, 4x, MSAA on. High: 2x, 8x, MSAA on. Ultra: 3x, 16x, MSAA on. Applies after pending GPU work finishes. Custom reflects individual changes."},
    {"Display mode","Choose a window, borderless desktop fullscreen, or exclusive fullscreen where supported. Applies immediately. Exclusive mode availability depends on the display."},
    {"Invincibility","Prevent normal damage from reducing the local player's health. Does not revive a dead player or prevent scripted mission failure."},
    {"Hide HUD","Hide the gameplay HUD for clean screenshots. Pause menus remain available so you can turn it back on."},
    {"Invisibility","Keep the local player in the game's native cloaked state. Turn off to restore normal visibility. Does not change other characters."},
    {"Direct mouse look","Use direct mouse camera control for aiming."},
    {"Sensitivity","Adjust how far the camera turns for each mouse movement."},
    {"Invert X","Reverse horizontal mouse movement."},
    {"Invert Y","Reverse vertical mouse movement."},
    {"Mouse menu navigation","Use the pointer to select options in game menus."},
    {"Mouse wheel aim zoom","Use the mouse wheel to adjust aiming zoom."},
    {"Aim trigger pressure","Set how strongly mouse aiming presses the virtual trigger."},
    {"Field of view","Show more or less of the world. Higher values widen the view. Updates the view while paused."},
    {"Menu / scripted camera cap (60 FPS)","Fallback 60 FPS cap for menus and scripted or locked cameras. Normal gameplay is unaffected. Cutscenes using the active high-FPS timing fix bypass this cap."},
    {"First-person body","Show your character's body when looking down in first person."},
    {"Visible holstered weapons","Show carried weapons attached to the character."},
    {"Third-person camera","View the player from behind instead of through their eyes."},
    {"Texture filtering","Keep distant and angled textures sharper. Higher filtering can increase GPU cost."},
    {"Preserve aspect ratio","Keep the picture's proportions, adding borders where needed instead of stretching."},
    {"Native 2x anti-aliasing","Smooth supported polygon edges using native 2x MSAA. Applies after pending GPU work finishes. Save settings to keep your choice."},
    {"Resolution scale","Increase internal rendering resolution. Applies after pending GPU work finishes. Higher values use more GPU power and memory. Save settings to keep your choice."},
    {"Shadow lift","Brighten dark tones while preserving black and white endpoints. Zero restores the original. Save and restart to apply."},
    {"VSync","Synchronize presentation to the display to reduce tearing. Applies immediately. Save settings to keep your choice."},
    {"Voice language","Choose the dialogue language. Requires the matching installed audio."},
    {"Skip intro (next launch)","Skip opening videos and go to the title screen on the next launch."},
    {"All weapon aim poses","Allow the additional aiming poses for supported weapons."},
    {"Automatic fire while zoomed","Allow automatic firing while aiming with zoom."},
    {"Sniper headshot gore","Enable the campaign sniper headshot gore effect."},
    {"Gore lifetime (minutes)","Set how long gore decals remain before being removed."},
    {"Training ammo refill","Refill ammunition in training."},
    {"Backpack reload","Unlocked. Reload a weapon from reserve ammunition when you put it away. Switch back to use the reloaded magazine. Works even with Manual Reload enabled."},
    {"Ignore weapon size","Ignore weapon size restrictions when carrying weapons."},
    {"UI size","Resize the PD menu and its text together. The menu stays within the display."},
    {"UI width","Widen or narrow the options column without stretching the text."},
    {"Reset UI layout","Restore the default menu size and width."},
    {"Save settings","Save these settings for future launches. Restart to apply options marked for restart."}
  };
  for(const auto& e:entries)if(e.label==label)return e.text;
  return "Select an option to read its description. Use the side arrows, LB/RB, or Page Up/Down to change tabs.";
}
}

namespace pdz_menu {
inline std::string_view ImpactFor(std::string_view label){
  if(label=="Play while shaders compile")return "Other loading or GPU hitches can still occur in either mode.";
  if(label=="Output resolution")return "Higher output sizes can increase GPU work. Internal resolution scale is a separate setting.";
  if(label=="Resolution scale")return "High GPU / VRAM cost. 2x draws four times as many pixels; 4x draws sixteen times as many. Higher values can substantially lower FPS.";
  if(label=="Graphics preset")return "High and Ultra increase GPU and VRAM demand. Expect lower FPS than Low or Medium when GPU-limited.";
  if(label=="Native 2x anti-aliasing")return "Moderate GPU and memory cost. May lower FPS.";
  if(label=="Texture filtering")return "Low to moderate GPU cost. Higher filtering may lower FPS.";
  if(label=="Field of view"||label=="Third-person camera")return "A wider view can draw more objects and lower FPS.";
  if(label=="First-person body"||label=="Visible holstered weapons")return "Additional character geometry can lower FPS.";
  if(label=="Sniper headshot gore"||label=="Gore lifetime (minutes)")return "More effects and retained decals increase memory and rendering cost. FPS may drop.";
  if(label=="Menu / scripted camera cap (60 FPS)")return "Limits presentation only in menus and eligible scripted cameras. Does not enable or disable physics timing fixes.";
  if(label=="VSync")return "Can cap FPS to the display refresh rate and affect latency.";
  if(label=="Shadow lift")return "Save and restart required.";
  return "No significant rendering cost expected.";
}
}
