#include "practice/practice_config.h"
#include "emu_log.h"
#include "front.h"
#include "practice_sram.h"
#include "practice_tests.h"
#include <bondconstants.h>
#include <fr.h>
#include <joy.h>
#include <lvl_text.h>
#include <textrelated.h>
#include <ultra64.h>

// Config storage is append-only. Add new fields to the end of PracticeConfig.
// Never delete or reorder fields; rename unused fields to deprecated_*.
struct PracticeConfig practice = {
    TRUE,          // skip_logos_on_startup
    TRUE,          // left_trigger_hotkeys
    LEVELID_TITLE, // boot_level
    FALSE,         // disable_intro_cutscenes
    5.0f,          // log_message_duration
    TRUE,          // show_hundredths_on_timer
    TRUE,          // show_mission_timer
    FALSE,         // grenade_cam
    TRUE,          // splits_enabled
    TRUE,          // gate_guard_status
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CONFIG_OFFSET(member) __builtin_offsetof(struct PracticeConfig, member)
#define SETTINGS_X 55
#define OPTIONS_X 224
#define OPTIONS_COLUMN_WIDTH 72
#define SETTINGS_LINE_HEIGHT 15
#define SETTINGS_VIEW_TOP 135
#define SETTINGS_BOTTOM_MARGIN 20
#define SETTINGS_FOCUS_MARGIN SETTINGS_LINE_HEIGHT
#define SETTINGS_HEADING_GAP 10
#define SETTINGS_SCROLLBAR_X 45
#define SETTINGS_SCROLLBAR_WIDTH 4
#define SETTINGS_SCROLLBAR_MARKER_HEIGHT 2
#define SETTINGS_SCROLLBAR_INSET 4
#define SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT 6
#define SETTINGS_SCROLLBAR_HALF_WIDTH (SETTINGS_SCROLLBAR_WIDTH / 2)
#define SETTINGS_SCROLLBAR_COLOR 0x00000055
#define MAX_OPTION_COLUMNS 2
#define SLIDER_REPEAT_DELAY 15
#define SLIDER_REPEAT_RATE 1
#define CONFIG_MAGIC 0x47434647 /* "GCFG" */
#define CONFIG_HEADER_SIZE (sizeof(u32) * 3)
#define TEXT_COLOR 0x000000FF
#define TEXT_DISABLED_COLOR 0x000000cc
#define TEXT_SELECTED_GLOW 0x999999FF
#define FOCUSED_OPTION_BOX_COLOR 0xddbb33aa

struct StoredPracticeConfig {
  u32 magic;
  u32 size;
  u32 checksum;
  struct PracticeConfig config;
};

typedef enum PracticeSettingType {
  PRACTICE_SETTING_OPTIONS,
  PRACTICE_SETTING_FLOAT_SLIDER
} PracticeSettingType;

typedef s32 (*PracticeSettingAppliesFn)(s32 stage_id);
typedef s32 (*PracticeSettingOptionsFn)(s32 stage_id, s32 option_index,
                                        const char **option_name,
                                        s32 *option_value);

struct PracticeOption {
  const char *name;
  s32 value;
};

struct PracticeSetting {
  const char *name;
  u32 value_offset;
  PracticeSettingType type;
  PracticeSettingAppliesFn applies;
  union {
    struct {
      const struct PracticeOption *items;
      s32 count;
      PracticeSettingOptionsFn get;
    } options;
    struct {
      f32 minimum;
      f32 maximum;
      f32 increment;
    } slider;
  } data;
};

#define OPTIONS_SETTING(label, member, choices, applies_fn)                    \
  {                                                                            \
    label, CONFIG_OFFSET(member), PRACTICE_SETTING_OPTIONS, applies_fn, {      \
      .options = { choices, ARRAY_COUNT(choices), NULL }                       \
    }                                                                          \
  }
#define DYNAMIC_OPTIONS_SETTING(label, member, get_options)                    \
  {                                                                            \
    label, CONFIG_OFFSET(member), PRACTICE_SETTING_OPTIONS, NULL, {            \
      .options = { NULL, 0, get_options }                                      \
    }                                                                          \
  }
#define SLIDER_SETTING(label, member, min, max, step)                          \
  {                                                                            \
    label, CONFIG_OFFSET(member), PRACTICE_SETTING_FLOAT_SLIDER, NULL, {       \
      .slider = { min, max, step }                                             \
    }                                                                          \
  }

typedef char StoredPracticeConfigMustFitReservedSpace
    [sizeof(struct StoredPracticeConfig) <= CONFIG_SRAM_SIZE ? 1 : -1];

static const struct PracticeOption s_left_right[] = {
    {"Left", TRUE},
    {"Right", FALSE},
};
static const struct PracticeOption s_disabled_enabled[] = {
    {"Disabled", FALSE},
    {"Enabled", TRUE},
};
static const struct PracticeOption s_enabled_disabled[] = {
    {"Enabled", TRUE},
    {"Disabled", FALSE},
};

static s32 splits_apply(s32 stage_id) { return stage_id == LEVELID_RUNWAY; }

static s32 dam_apply(s32 stage_id) { return stage_id == LEVELID_DAM; }

static const char *level_name(s32 stage_id) {
  s32 i;

  if (stage_id == LEVELID_TITLE || stage_id == LEVELID_NONE) {
    return "Menu";
  }

  for (i = 0; mission_folder_setup_entries[i].folder_text_preset != 0; i++) {
    if (mission_folder_setup_entries[i].stage_id == stage_id) {
      return langGet(mission_folder_setup_entries[i].icon_text_preset != 0
                         ? mission_folder_setup_entries[i].icon_text_preset
                         : mission_folder_setup_entries[i].folder_text_preset);
    }
  }

  return "Unknown";
}

static s32 boot_level_options(s32 stage_id, s32 option_index,
                              const char **option_name, s32 *option_value) {
  switch (option_index) {
  case 0:
    *option_name = level_name(stage_id);
    *option_value = stage_id;
    return TRUE;
  case 1:
    *option_value =
        practice.boot_level == stage_id ? LEVELID_TITLE : practice.boot_level;
    *option_name = level_name(*option_value);
    return TRUE;
  }
  return FALSE;
}

static const struct PracticeSetting s_level_settings[] = {
    OPTIONS_SETTING("Splits", splits_enabled, s_enabled_disabled, splits_apply),
    OPTIONS_SETTING("Gate guard status", gate_guard_status, s_enabled_disabled,
                    dam_apply),
    DYNAMIC_OPTIONS_SETTING("Boot into level", boot_level, boot_level_options),
};

static const struct PracticeSetting s_global_settings[] = {
    SLIDER_SETTING("Log message duration", log_message_duration, 0.1f, 20.0f,
                   0.1f),
    OPTIONS_SETTING("Skip logos on startup", skip_logos_on_startup,
                    s_enabled_disabled, NULL),
    OPTIONS_SETTING("Hotkey trigger", left_trigger_hotkeys, s_left_right, NULL),
    OPTIONS_SETTING("Grenade cam", grenade_cam, s_disabled_enabled, NULL),
};

#undef OPTIONS_SETTING
#undef DYNAMIC_OPTIONS_SETTING
#undef SLIDER_SETTING

static s32 s_focused_visible_setting;
static s32 s_focused_option;
static s32 s_scroll_offset;
static s32 s_objectives_bottom;
static s32 s_slider_hold_frames;
static u16 s_slider_hold_button;
static s32 s_last_stage = LEVELID_NONE;
static s32 s_config_dirty;

extern Gfx *frontPrintText(Gfx *gdl, s32 *x, s32 *y, s8 *text,
                           s32 second_font_table, s32 first_font_table,
                           s32 color, s32 view_x, s32 view_y, s32 arg9,
                           s32 arga);
extern int sprintf(char *dst, const char *fmt, ...);

static void *setting_value(const struct PracticeSetting *setting) {
  return (u8 *)&practice + setting->value_offset;
}

static u32 config_checksum(const void *data, u32 size) {
  const u8 *bytes = data;
  u32 checksum = 2166136261U;
  u32 i;

  for (i = 0; i < size; i++) {
    checksum = (checksum ^ bytes[i]) * 16777619U;
  }
  return checksum;
}

static void save_config(void) {
  struct StoredPracticeConfig stored;

  stored.magic = CONFIG_MAGIC;
  stored.size = sizeof(stored.config);
  stored.config = practice;
  stored.checksum = config_checksum(&stored.config, stored.size);
  if (sram_write(CONFIG_SRAM_OFFSET, &stored, sizeof(stored)) == 0) {
    s_config_dirty = FALSE;
  }
}

static void apply_build_overrides(void) {
#ifdef PRACTICE_TEST_BOOT_LEVEL
  practice.boot_level = PRACTICE_TEST_BOOT_LEVEL;
#endif
#ifdef PRACTICE_TEST_SKIP_INTRO
  practice.disable_intro_cutscenes = PRACTICE_TEST_SKIP_INTRO;
#endif
}

void practice_config_load(void) {
  u8 config_sram_bytes[CONFIG_SRAM_SIZE];
  struct StoredPracticeConfig *stored =
      (struct StoredPracticeConfig *)&config_sram_bytes;
  u32 size = CONFIG_HEADER_SIZE + CONFIG_HEADER_SIZE - CONFIG_HEADER_SIZE % 16;

  if (sram_read(CONFIG_SRAM_OFFSET, &config_sram_bytes, size) == 0 &&
      stored->magic == CONFIG_MAGIC && stored->size != 0 &&
      stored->size <= sizeof(stored->config) &&
      stored->size + CONFIG_HEADER_SIZE <= CONFIG_SRAM_SIZE) {
    stored->config = practice;
    if (sram_read(CONFIG_SRAM_OFFSET + CONFIG_HEADER_SIZE, &stored->config,
                  stored->size) == 0 &&
        stored->checksum == config_checksum(&stored->config, stored->size)) {
      practice = stored->config;
    }
  }

  apply_build_overrides();
}

static s32 setting_applies(const struct PracticeSetting *setting,
                           s32 stage_id) {
  return setting->applies == NULL || setting->applies(stage_id);
}

static s32 get_option(const struct PracticeSetting *setting, s32 stage_id,
                      s32 option_index, const char **name, s32 *value) {
  const struct PracticeOption *option;

  if (setting->data.options.get != NULL) {
    return setting->data.options.get(stage_id, option_index, name, value);
  }

  if (option_index < 0 || option_index >= setting->data.options.count) {
    return FALSE;
  }
  option = &setting->data.options.items[option_index];
  *name = option->name;
  *value = option->value;
  return TRUE;
}

static s32 applicable_setting_count(const struct PracticeSetting *settings,
                                    s32 setting_count, s32 stage_id) {
  s32 i;
  s32 count = 0;

  for (i = 0; i < setting_count; i++) {
    if (setting_applies(&settings[i], stage_id)) {
      count++;
    }
  }
  return count;
}

static const struct PracticeSetting *
applicable_setting(const struct PracticeSetting *settings, s32 setting_count,
                   s32 applicable_index, s32 stage_id) {
  s32 i;

  for (i = 0; i < setting_count; i++) {
    if (setting_applies(&settings[i], stage_id) && applicable_index-- == 0) {
      return &settings[i];
    }
  }
  return NULL;
}

static s32 level_setting_count(s32 stage_id) {
  return applicable_setting_count(s_level_settings,
                                  ARRAY_COUNT(s_level_settings), stage_id);
}

static s32 visible_setting_count(s32 stage_id) {
  return level_setting_count(stage_id) +
         applicable_setting_count(s_global_settings,
                                  ARRAY_COUNT(s_global_settings), stage_id);
}

static const struct PracticeSetting *visible_setting(s32 visible_index,
                                                     s32 stage_id) {
  s32 level_count = level_setting_count(stage_id);

  if (visible_index < level_count) {
    return applicable_setting(s_level_settings, ARRAY_COUNT(s_level_settings),
                              visible_index, stage_id);
  }
  return applicable_setting(s_global_settings, ARRAY_COUNT(s_global_settings),
                            visible_index - level_count, stage_id);
}

static void sync_focused_option(s32 stage_id) {
  const struct PracticeSetting *setting =
      visible_setting(s_focused_visible_setting, stage_id);
  const char *name;
  s32 value;
  s32 i;

  s_focused_option = 0;
  if (setting == NULL || setting->type == PRACTICE_SETTING_FLOAT_SLIDER) {
    return;
  }

  for (i = 0; i < MAX_OPTION_COLUMNS &&
              get_option(setting, stage_id, i, &name, &value);
       i++) {
    if (*(s32 *)setting_value(setting) == value) {
      s_focused_option = i;
      return;
    }
  }
}

static s32 setting_row_y(s32 visible_index, s32 stage_id) {
  s32 level_count = level_setting_count(stage_id);
  s32 y = s_objectives_bottom + SETTINGS_HEADING_GAP;

  if (level_count > 0) {
    y += SETTINGS_LINE_HEIGHT;
    if (visible_index < level_count) {
      return y + visible_index * SETTINGS_LINE_HEIGHT;
    }
    y += level_count * SETTINGS_LINE_HEIGHT + SETTINGS_HEADING_GAP;
  }

  y += SETTINGS_LINE_HEIGHT;
  return y + (visible_index - level_count) * SETTINGS_LINE_HEIGHT;
}

static void keep_focus_visible(s32 stage_id) {
  s32 y = setting_row_y(s_focused_visible_setting, stage_id) + s_scroll_offset;
  s32 top = SETTINGS_VIEW_TOP + SETTINGS_FOCUS_MARGIN;
  s32 bottom = practice_config_menu_view_bottom() - SETTINGS_FOCUS_MARGIN;

  if (y > bottom) {
    s_scroll_offset -= y - bottom;
  } else if (y < top) {
    s_scroll_offset += top - y;
    if (s_scroll_offset > 0) {
      s_scroll_offset = 0;
    }
  }
}

static void change_setting(s32 direction, s32 stage_id) {
  const struct PracticeSetting *setting =
      visible_setting(s_focused_visible_setting, stage_id);

  if (setting == NULL) {
    return;
  }

  if (setting->type == PRACTICE_SETTING_FLOAT_SLIDER) {
    f32 *value = setting_value(setting);
    s32 steps = (s32)((*value - setting->data.slider.minimum) /
                          setting->data.slider.increment +
                      0.5f);
    s32 max_steps =
        (s32)((setting->data.slider.maximum - setting->data.slider.minimum) /
                  setting->data.slider.increment +
              0.5f);

    steps += direction;
    if (steps < 0) {
      steps = 0;
    } else if (steps > max_steps) {
      steps = max_steps;
    }
    *value =
        setting->data.slider.minimum + steps * setting->data.slider.increment;
    s_config_dirty = TRUE;
  } else {
    const char *name;
    s32 value;
    s32 next = s_focused_option + direction;

    if (next < 0) {
      next = 0;
    } else if (next >= MAX_OPTION_COLUMNS) {
      next = MAX_OPTION_COLUMNS - 1;
    }
    if (!get_option(setting, stage_id, next, &name, &value)) {
      return;
    }
    s_focused_option = next;
    *(s32 *)setting_value(setting) = value;
    s_config_dirty = TRUE;
  }
}

void practice_config_menu_reset(void) {
  s_focused_visible_setting = 0;
  s_focused_option = 0;
  s_scroll_offset = 0;
  s_objectives_bottom = 167;
  s_slider_hold_frames = 0;
  s_slider_hold_button = 0;
  s_last_stage = LEVELID_NONE;
}

void practice_config_menu_tick(s32 stage_id, s32 is_objectives_page) {
  s32 count;
  u16 pressed;
  u16 held;
  u16 direction_button;
  s32 direction;

  if (!is_objectives_page) {
    if (s_config_dirty) {
      save_config();
    }
    s_slider_hold_frames = 0;
    s_slider_hold_button = 0;
    return;
  }

  if (stage_id != s_last_stage) {
    s_focused_visible_setting = 0;
    s_scroll_offset = 0;
    s_last_stage = stage_id;
    sync_focused_option(stage_id);
  }

  keep_focus_visible(stage_id);
  count = visible_setting_count(stage_id);
  pressed = joyGetButtonsPressedThisFrame(
      PLAYER_1, U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS);

  if (pressed & U_CBUTTONS) {
    if (s_focused_visible_setting > 0) {
      s_focused_visible_setting--;
      sync_focused_option(stage_id);
      keep_focus_visible(stage_id);
    } else if (s_scroll_offset < 0) {
      s_scroll_offset += SETTINGS_LINE_HEIGHT;
      if (s_scroll_offset > 0) {
        s_scroll_offset = 0;
      }
    }
  } else if ((pressed & D_CBUTTONS) && s_focused_visible_setting < count - 1) {
    s_focused_visible_setting++;
    sync_focused_option(stage_id);
    keep_focus_visible(stage_id);
  }

  direction_button = 0;
  direction = 0;
  if (pressed & L_CBUTTONS) {
    direction_button = L_CBUTTONS;
    direction = -1;
  } else if (pressed & R_CBUTTONS) {
    direction_button = R_CBUTTONS;
    direction = 1;
  }

  if (direction_button != 0) {
    const struct PracticeSetting *setting =
        visible_setting(s_focused_visible_setting, stage_id);
    change_setting(direction, stage_id);
    if (setting->type != PRACTICE_SETTING_FLOAT_SLIDER && s_config_dirty) {
      save_config();
    }
    s_slider_hold_button = direction_button;
    s_slider_hold_frames = 0;
    return;
  }

  held = joyGetButtons(PLAYER_1, L_CBUTTONS | R_CBUTTONS);
  if (held == s_slider_hold_button && held != 0 &&
      visible_setting(s_focused_visible_setting, stage_id)->type ==
          PRACTICE_SETTING_FLOAT_SLIDER) {
    s_slider_hold_frames++;
    if (s_slider_hold_frames >= SLIDER_REPEAT_DELAY &&
        (s_slider_hold_frames - SLIDER_REPEAT_DELAY) % SLIDER_REPEAT_RATE ==
            0) {
      change_setting(held == L_CBUTTONS ? -1 : 1, stage_id);
    }
  } else {
    if (s_config_dirty) {
      save_config();
    }
    s_slider_hold_frames = 0;
    s_slider_hold_button = 0;
  }
}

s32 practice_config_menu_scroll_offset(void) { return s_scroll_offset; }

s32 practice_config_menu_view_bottom(void) {
  return viGetY() - SETTINGS_BOTTOM_MARGIN;
}

static s32 line_is_below_view(s32 y) { return y > viGetY(); }

void practice_config_menu_set_objectives_bottom(s32 y) {
  s_objectives_bottom = y - s_scroll_offset;
}

static Gfx *print_text(Gfx *gdl, s32 x, s32 y, const char *text, s32 color) {
  if (y < SETTINGS_VIEW_TOP || line_is_below_view(y)) {
    return gdl;
  }
  return frontPrintText(gdl, &x, &y, (s8 *)text, ptrFontZurichBoldChars,
                        ptrFontZurichBold, color, viGetX(), viGetY(), 0, 0);
}

static Gfx *print_option(Gfx *gdl, s32 x, s32 y, const char *text, s32 selected,
                         s32 focused) {
  s32 width;
  s32 height;

  if (y < SETTINGS_VIEW_TOP || line_is_below_view(y)) {
    return gdl;
  }

  if (focused) {
    textMeasure(&height, &width, (char *)text,
                (struct fontchar *)ptrFontZurichBoldChars,
                (struct font *)ptrFontZurichBold, 0);
    gdl = microcode_constructor_related_to_menus(
        gdl, x - 4, y - 1, x + width + 4, y + SETTINGS_LINE_HEIGHT,
        FOCUSED_OPTION_BOX_COLOR);
  }

  if (selected) {
    return textRenderGlow(gdl, &x, &y, (s8 *)text, ptrFontZurichBoldChars,
                          ptrFontZurichBold, TEXT_COLOR, TEXT_SELECTED_GLOW,
                          viGetX(), viGetY(), 0, 0);
  }

  return frontPrintText(
      gdl, &x, &y, (s8 *)text, ptrFontZurichBoldChars, ptrFontZurichBold,
      selected ? TEXT_COLOR : TEXT_DISABLED_COLOR, viGetX(), viGetY(), 0, 0);
}

static Gfx *render_setting(Gfx *gdl, const struct PracticeSetting *setting,
                           s32 visible_index, s32 stage_id, s32 y) {
  s32 focused = visible_index == s_focused_visible_setting;

  gdl = print_text(gdl, SETTINGS_X, y, setting->name, TEXT_COLOR);

  if (setting->type == PRACTICE_SETTING_FLOAT_SLIDER) {
    char slider[32];
    f32 value = *(f32 *)setting_value(setting);
    s32 position =
        (s32)(((value - setting->data.slider.minimum) * 10.0f) /
              (setting->data.slider.maximum - setting->data.slider.minimum));
    s32 i;

    if (position < 0) {
      position = 0;
    } else if (position > 10) {
      position = 10;
    }
    slider[0] = '[';
    for (i = 0; i <= 10; i++) {
      slider[i + 1] = i == position ? '|' : '-';
    }
    slider[12] = ']';
    sprintf(&slider[13], " %.1f", value);
    gdl = print_option(gdl, OPTIONS_X, y, slider, TRUE, focused);
  } else {
    const char *name;
    s32 value;
    s32 selected_value = *(s32 *)setting_value(setting);
    s32 x = OPTIONS_X;
    s32 i;

    for (i = 0; i < MAX_OPTION_COLUMNS &&
                get_option(setting, stage_id, i, &name, &value);
         i++) {
      s32 selected = value == selected_value;
      s32 option_focused = focused && i == s_focused_option;

      gdl = print_option(gdl, x, y, name, selected, option_focused);
      x += OPTIONS_COLUMN_WIDTH;
    }
  }
  return gdl;
}

static Gfx *render_settings(Gfx *gdl, const char *heading,
                            const struct PracticeSetting *settings,
                            s32 setting_count, s32 stage_id, s32 *visible_index,
                            s32 *y) {
  s32 i;

  if (line_is_below_view(*y)) {
    return gdl;
  }
  gdl = print_text(gdl, SETTINGS_X, *y, heading, TEXT_COLOR);
  *y += SETTINGS_LINE_HEIGHT;

  for (i = 0; i < setting_count; i++) {
    if (setting_applies(&settings[i], stage_id)) {
      if (line_is_below_view(*y)) {
        return gdl;
      }
      gdl = render_setting(gdl, &settings[i], (*visible_index)++, stage_id, *y);
      *y += SETTINGS_LINE_HEIGHT;
    }
  }
  return gdl;
}

static Gfx *render_scrollbar(Gfx *gdl, s32 stage_id) {
  s32 setting_count = visible_setting_count(stage_id);
  s32 view_bottom = practice_config_menu_view_bottom();
  s32 view_height = view_bottom - SETTINGS_VIEW_TOP + 1;
  s32 content_bottom;
  s32 content_height;
  s32 max_scroll;
  s32 scroll;
  s32 track_top;
  s32 track_height;
  s32 thumb_height;
  s32 thumb_travel;
  s32 thumb_y;

  if (setting_count == 0 || view_height <= 0) {
    return gdl;
  }

  content_bottom =
      setting_row_y(setting_count - 1, stage_id) + SETTINGS_LINE_HEIGHT;
  content_height = content_bottom - SETTINGS_VIEW_TOP;
  if (content_height <= view_height) {
    return gdl;
  }

  track_top = SETTINGS_VIEW_TOP + SETTINGS_SCROLLBAR_INSET;
  track_height = view_height - SETTINGS_SCROLLBAR_INSET * 2;
  thumb_height = track_height * view_height / content_height;
  if (thumb_height < SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT) {
    thumb_height = SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT;
  }

  max_scroll = content_height - view_height;
  scroll = -s_scroll_offset;
  if (scroll < 0) {
    scroll = 0;
  } else if (scroll > max_scroll) {
    scroll = max_scroll;
  }
  thumb_travel = track_height - thumb_height;
  thumb_y = track_top + scroll * thumb_travel / max_scroll;

  gdl = microcode_constructor_related_to_menus(
      gdl, SETTINGS_SCROLLBAR_X - SETTINGS_SCROLLBAR_HALF_WIDTH,
      SETTINGS_VIEW_TOP - SETTINGS_SCROLLBAR_HALF_WIDTH,
      SETTINGS_SCROLLBAR_X + SETTINGS_SCROLLBAR_HALF_WIDTH,
      SETTINGS_VIEW_TOP + SETTINGS_SCROLLBAR_HALF_WIDTH,
      SETTINGS_SCROLLBAR_COLOR);
  gdl = microcode_constructor_related_to_menus(
      gdl, SETTINGS_SCROLLBAR_X - SETTINGS_SCROLLBAR_HALF_WIDTH,
      view_bottom - SETTINGS_SCROLLBAR_HALF_WIDTH,
      SETTINGS_SCROLLBAR_X + SETTINGS_SCROLLBAR_HALF_WIDTH,
      view_bottom + SETTINGS_SCROLLBAR_HALF_WIDTH, SETTINGS_SCROLLBAR_COLOR);
  return microcode_constructor_related_to_menus(
      gdl, SETTINGS_SCROLLBAR_X - SETTINGS_SCROLLBAR_HALF_WIDTH, thumb_y,
      SETTINGS_SCROLLBAR_X + SETTINGS_SCROLLBAR_HALF_WIDTH,
      thumb_y + thumb_height - 1, SETTINGS_SCROLLBAR_COLOR);
}

Gfx *practice_config_menu_render(Gfx *gdl, s32 stage_id) {
  s32 level_count = level_setting_count(stage_id);
  s32 visible_index = 0;
  s32 y = s_objectives_bottom + SETTINGS_HEADING_GAP + s_scroll_offset;

  gdl = render_scrollbar(gdl, stage_id);

  if (level_count > 0) {
    gdl = render_settings(gdl, "LEVEL SETTINGS:", s_level_settings,
                          ARRAY_COUNT(s_level_settings), stage_id,
                          &visible_index, &y);
    y += SETTINGS_HEADING_GAP;
  }

  return render_settings(gdl, "GLOBAL SETTINGS:", s_global_settings,
                         ARRAY_COUNT(s_global_settings), stage_id,
                         &visible_index, &y);
}
