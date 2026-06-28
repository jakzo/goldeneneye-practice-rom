#ifndef PRACTICE_CONFIG_H
#define PRACTICE_CONFIG_H

#include <ultra64.h>

/*
 * Persistent config is append-only. Add new options to the end of this struct.
 * Never delete or reorder an option; rename unused options to deprecated_*.
 */
struct PracticeConfig {
  s32 skip_logos_on_startup;
  s32 left_trigger_hotkeys;
  s32 boot_level;
  s32 disable_intro_cutscenes;
  f32 log_message_duration;
  s32 show_hundredths_on_timer;
  s32 show_mission_timer;
  s32 grenade_cam;
  s32 splits_enabled;
};

extern struct PracticeConfig practice;

void practice_config_load(void);
void practice_config_menu_reset(void);
void practice_config_menu_tick(s32 stage_id, s32 is_objectives_page);
s32 practice_config_menu_scroll_offset(void);
s32 practice_config_menu_view_bottom(void);
void practice_config_menu_set_objectives_bottom(s32 y);
Gfx *practice_config_menu_render(Gfx *gdl, s32 stage_id);

#endif /* PRACTICE_CONFIG_H */
