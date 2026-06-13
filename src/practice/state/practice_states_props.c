#include "practice_states_props.h"
#include "practice_states_utils.h"
#include "chrai.h"
#include <bondconstants.h>
#include <ultra64.h>

void save_props_state(SavedPropsState *dst) {
    s32 i;

    for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
        PropRecord *prop = get_prop_by_index(i);

        if (prop != NULL && (prop->flags & PROPFLAG_ENABLED)) {
            switch ((PROP_TYPE)prop->type) {
                case PROP_TYPE_NUL:
                    break;
                case PROP_TYPE_OBJ:
                    break;
                case PROP_TYPE_DOOR:
                    break;
                case PROP_TYPE_CHR:
                    break;
                case PROP_TYPE_WEAPON:
                    break;
                case PROP_TYPE_PLAYER:
                    break;
                case PROP_TYPE_VIEWER:
                    break;
                case PROP_TYPE_EXPLOSION:
                    break;
                case PROP_TYPE_SMOKE:
                    break;
                default:
                    break;
            }
        }
    }
}

void load_props_state(SavedPropsState *src) {
    s32 i;

    for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
        PropRecord *prop = get_prop_by_index(i);

        if (prop != NULL && (prop->flags & PROPFLAG_ENABLED)) {
            switch ((PROP_TYPE)prop->type) {
                case PROP_TYPE_NUL:
                    break;
                case PROP_TYPE_OBJ:
                    break;
                case PROP_TYPE_DOOR:
                    break;
                case PROP_TYPE_CHR:
                    break;
                case PROP_TYPE_WEAPON:
                    break;
                case PROP_TYPE_PLAYER:
                    break;
                case PROP_TYPE_VIEWER:
                    break;
                case PROP_TYPE_EXPLOSION:
                    break;
                case PROP_TYPE_SMOKE:
                    break;
                default:
                    break;
            }
        }
    }
}
