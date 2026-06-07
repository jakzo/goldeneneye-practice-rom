#ifndef PRACTICE_CONFIG_H
#define PRACTICE_CONFIG_H

#include <ultra64.h>

struct PracticeConfig {
    s32 skip_logos_on_startup;
    s32 left_trigger_hotkeys;
    s32 boot_level;
    s32 disable_intro_cutscenes;
};

extern struct PracticeConfig practice;

#endif /* PRACTICE_CONFIG_H */
