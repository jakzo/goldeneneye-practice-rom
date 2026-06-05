#include <os_extension.h>
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <joy.h>
#include "player_2.h"
#include "practice_states.h"
#include "practice_config.h"

struct HotkeysLayout {
    u16 trigger;
    u16 up;
    u16 down;
    u16 left;
    u16 right;
};

static const struct HotkeysLayout g_hotkeys_normal = {
    L_TRIG,
    U_JPAD,
    D_JPAD,
    L_JPAD,
    R_JPAD,
};

static const struct HotkeysLayout g_hotkeys_left_handed = {
    R_TRIG,
    U_CBUTTONS,
    D_CBUTTONS,
    L_CBUTTONS,
    R_CBUTTONS,
};

bool practice_check_hotkeys(void)
{
    const struct HotkeysLayout *keys = practice.left_handed_hotkeys
        ? &g_hotkeys_left_handed
        : &g_hotkeys_normal;

    u16 jgb = joyGetButtons(get_cur_playernum(), ANY_BUTTON);
    u16 jgbptf = joyGetButtonsPressedThisFrame(get_cur_playernum(), ANY_BUTTON);

    if ((jgb & keys->trigger) && (jgbptf & keys->down)) {
        save_game_state();
        return TRUE;
    }
    if ((jgb & keys->trigger) && (jgbptf & keys->up)) {
        load_game_state();
        return TRUE;
    }
    return FALSE;
}
