#include <os_extension.h>
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <joy.h>
#include "player_2.h"
#include "practice_states.h"

bool practice_check_hotkeys(void)
{
    u16 raw_jgb = joyGetButtons(get_cur_playernum(), ANY_BUTTON);
    u16 raw_jgbptf = joyGetButtonsPressedThisFrame(get_cur_playernum(), ANY_BUTTON);
    if ((raw_jgb & L_TRIG) && (raw_jgbptf & D_JPAD)) {
        save_game_state();
        return TRUE;
    }
    if ((raw_jgb & L_TRIG) && (raw_jgbptf & U_JPAD)) {
        load_game_state();
        return TRUE;
    }
    return FALSE;
}
