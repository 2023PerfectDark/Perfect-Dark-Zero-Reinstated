// game_cvars.h - Game-specific CVAR declarations
#pragma once

#include <rex/cvar.h>
REXCVAR_DECLARE(bool, pdz_skip_intro);
REXCVAR_DECLARE(int32_t, pdz_dub_language);

REXCVAR_DECLARE(std::string, graphics_backend);
REXCVAR_DECLARE(bool, pdz_fps60_unlock);
REXCVAR_DECLARE(bool, pdz_aspect_ratio_16_9);
REXCVAR_DECLARE(int32_t, pdz_gpu_wait_mode);
REXCVAR_DECLARE(bool, dev_debug_runtime);
REXCVAR_DECLARE(bool, pdz_mousehook);
REXCVAR_DECLARE(double, pdz_mouse_sensitivity);
REXCVAR_DECLARE(bool, pdz_mouse_invert_x);
REXCVAR_DECLARE(bool, pdz_mouse_invert_y);
REXCVAR_DECLARE(double, pdz_fov);
REXCVAR_DECLARE(bool, pdz_mouse_menu_navigation);
REXCVAR_DECLARE(bool, pdz_menu_mode);
REXCVAR_DECLARE(bool, pdz_wheel_ads);
REXCVAR_DECLARE(int32_t, pdz_ads_pressure);
REXCVAR_DECLARE(bool, pdz_ignore_weapon_size);

REXCVAR_DECLARE(bool, pdz_all_weapon_ads);

REXCVAR_DECLARE(bool, pdz_cutscene_60fps);
REXCVAR_DECLARE(bool, pdz_physics_timing_fix);
REXCVAR_DECLARE(bool, pdz_visible_holsters);
REXCVAR_DECLARE(bool, pdz_first_person_body);
REXCVAR_DECLARE(int32_t, pdz_campaign_rules);
REXCVAR_DECLARE(bool, pdz_training_ammo_refill);
REXCVAR_DECLARE(bool, pdz_campaign_no_spread);
REXCVAR_DECLARE(bool, pdz_campaign_zoom_auto);
REXCVAR_DECLARE(bool, pdz_campaign_gore);
REXCVAR_DECLARE(int32_t, pdz_gore_decal_minutes);
REXCVAR_DECLARE(bool, pdz_coop_ally);
REXCVAR_DECLARE(bool, pdz_coop_native_partner);
REXCVAR_DECLARE(bool, pdz_tp_enable);
REXCVAR_DECLARE(double, pdz_tp_offset_x);
REXCVAR_DECLARE(double, pdz_tp_offset_y);
REXCVAR_DECLARE(double, pdz_tp_offset_z);
REXCVAR_DECLARE(bool, pdz_tp_auto_fp_gadgets);
REXCVAR_DECLARE(int32_t, pdz_tp_zoom_mode);
REXCVAR_DECLARE(bool, pdz_tp_binoculars_fp);

REXCVAR_DECLARE(double, pdz_ui_scale);
REXCVAR_DECLARE(double, pdz_ui_width);

REXCVAR_DECLARE(bool, pdz_hide_hud);
REXCVAR_DECLARE(bool, pdz_invisibility);

REXCVAR_DECLARE(bool, pdz_invincibility);
