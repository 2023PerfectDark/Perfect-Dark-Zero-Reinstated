#include "game_cvars.h"
REXCVAR_DEFINE_INT32(head_roll, 1, "Gameplay", "Modern/Classic breathing sway and head roll: 0 off, 1 on").range(0,1);
REXCVAR_DEFINE_INT32(no_head_roll, 0, "Gameplay", "Legacy compatibility: 1 disables head roll when head_roll is not explicitly set").range(0,1);
REXCVAR_DEFINE_BOOL(pdz_skip_intro, true, "Gameplay", "Skip startup intro movies and open the title screen");
REXCVAR_DEFINE_BOOL(pdz_ignore_weapon_size, false, "Gameplay", "Experimental: ignore weapon size; retain the 13-object inventory storage bound");

REXCVAR_DEFINE_BOOL(pdz_mouse_menu_navigation, true, "Gameplay", "Mouse movement/wheel selects menu options; click accepts; right-click backs out");
REXCVAR_DEFINE_BOOL(pdz_menu_mode, false, "Gameplay", "Force mouse menu navigation (F8); disable to resume camera look");
REXCVAR_DEFINE_BOOL(pdz_wheel_ads, true, "Gameplay", "Right mouse + wheel adjusts analog aim trigger pressure");
REXCVAR_DEFINE_INT32(pdz_ads_pressure, 128, "Gameplay", "Mouse aim trigger pressure; wheel changes this while aiming")
    .range(32, 255);

REXCVAR_DEFINE_BOOL(pdz_mousehook, true, "Gameplay", "Direct mouse camera control (retail TU0)");
REXCVAR_DEFINE_DOUBLE(pdz_mouse_sensitivity, 1.0, "Gameplay", "Direct mouse sensitivity")
    .range(0.05, 10.0);
REXCVAR_DEFINE_BOOL(pdz_mouse_invert_x, false, "Gameplay", "Invert mouse horizontal look");
REXCVAR_DEFINE_BOOL(pdz_mouse_invert_y, false, "Gameplay", "Invert mouse vertical look");
REXCVAR_DEFINE_DOUBLE(pdz_fov, 58.0, "Gameplay", "FOV scale: original 58, Xenia patch 100")
    .range(40.0, 120.0);

REXCVAR_DEFINE_STRING(graphics_backend, "any", "GPU",
                      "Graphics API backend: any, d3d12, vulkan")
    .allowed({"any", "d3d12", "vulkan"});

REXCVAR_DEFINE_BOOL(pdz_fps60_unlock, false, "Gameplay",
                    "Legacy native presentation patch; retained for compatibility with saved settings");

REXCVAR_DEFINE_BOOL(pdz_aspect_ratio_16_9, false, "Gameplay",
                    "Enable 16:9 aspect ratio");

REXCVAR_DEFINE_INT32(pdz_gpu_wait_mode, 1, "Performance",
                     "Pause between polls while the game waits for free command-buffer space: "
                     "0 = busy spin (original), 1 = yield the core, 2 = sleep 200us")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(dev_debug_runtime, false, "Debug",
                    "Enable runtime debug tools (stub sweep, missing function scan)");

REXCVAR_DEFINE_BOOL(pdz_all_weapon_ads, false, "Gameplay", "Experimental: native weapon aim pose while holding RMB at minimum zoom");

REXCVAR_DEFINE_BOOL(pdz_cutscene_60fps, true, "Gameplay", "Cap scripted/locked cameras and front-end to 60 FPS; normal gameplay remains unrestricted");
REXCVAR_DEFINE_BOOL(pdz_physics_timing_fix, true, "Gameplay", "Keep physics solver steps aligned with collider updates above 60 FPS");
REXCVAR_DEFINE_BOOL(pdz_visible_holsters, true, "Gameplay", "Show stowed weapons on Joanna's back and thighs in solo campaign");
REXCVAR_DEFINE_BOOL(pdz_first_person_body, false, "Gameplay", "Experimental body visibility; may flicker and overlap first-person arms");
REXCVAR_DEFINE_INT32(pdz_campaign_rules, 0, "Gameplay", "Campaign rules: 0 Original, 1 Modern, 2 Classic").range(0, 2);
REXCVAR_DEFINE_BOOL(pdz_training_ammo_refill, false, "Gameplay", "Cheat: enable DataCore's timed pistol ammo refill to 99");
REXCVAR_DEFINE_BOOL(pdz_campaign_no_spread, true, "Gameplay", "Modern/Classic: remove local-player bullet aiming spread");
REXCVAR_DEFINE_BOOL(pdz_campaign_zoom_auto, true, "Gameplay", "Modern/Classic: preserve automatic fire while zoomed; tap for single shots");
REXCVAR_DEFINE_BOOL(pdz_coop_ally, false, "Gameplay", "Experimental offline campaign companion (native integration test)");
REXCVAR_DEFINE_BOOL(pdz_coop_native_partner, false, "Debug", "Test native Player 2 joining for Bot Co-op (development only)");
REXCVAR_DEFINE_BOOL(pdz_tp_enable, false, "Gameplay",
                    "Third-person camera (toggle with bind_third_person)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_x, 0.0, "Gameplay",
                      "Third-person camera offset, camera-local X")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_y, 0.0, "Gameplay",
                      "Third-person camera offset, camera-local Y")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_z, 0.0, "Gameplay",
                      "Third-person camera offset, camera-local Z")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(pdz_tp_auto_fp_gadgets, true, "Gameplay",
                    "Third person: switch to first person while the Data Thief is in use")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_INT32(pdz_tp_zoom_mode, 2, "Gameplay",
                     "Third-person camera while zoomed (scopes, binoculars): "
                     "0 = stay behind, 1 = first person, 2 = slide forward along the view line")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(pdz_tp_binoculars_fp, false, "Gameplay",
                    "Third person: look through the binoculars in first person "
                    "instead of like scopes (pdz_tp_zoom_mode)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_ui_scale, 1.0, "Interface", "PD menu size") .range(0.65, 1.4);
REXCVAR_DEFINE_DOUBLE(pdz_ui_width, 1.0, "Interface", "PD menu option column width") .range(0.85, 1.4);

REXCVAR_DEFINE_BOOL(pdz_hide_hud, false, "Interface", "Hide gameplay HUD for screenshots");
REXCVAR_DEFINE_BOOL(pdz_invisibility, false, "Cheats", "Keep the local player cloaked");

REXCVAR_DEFINE_BOOL(pdz_invincibility, false, "Cheats", "Prevent local player health loss");

REXCVAR_DEFINE_BOOL(pdz_custom_mode, false, "Challenges", "Perfect Dark Zero custom mission mode");
REXCVAR_DEFINE_DOUBLE(pdz_enemy_health, 100.0, "Challenges", "Enemy health percentage").range(10.0, 1000.0);
REXCVAR_DEFINE_DOUBLE(pdz_enemy_damage, 100.0, "Challenges", "Enemy damage percentage").range(10.0, 1000.0);
REXCVAR_DEFINE_DOUBLE(pdz_enemy_accuracy, 100.0, "Challenges", "Enemy accuracy percentage").range(10.0, 1000.0);
REXCVAR_DEFINE_DOUBLE(pdz_enemy_reaction, 100.0, "Challenges", "Enemy reaction percentage").range(10.0, 1000.0);
