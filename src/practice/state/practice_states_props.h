#ifndef PRACTICE_STATES_PROPS_H
#define PRACTICE_STATES_PROPS_H

#include <ultra64.h>
#include <bondtypes.h>

typedef struct {
    u32 dummy;
} SavedPropsState;

void save_props_state(SavedPropsState *dst);
void load_props_state(SavedPropsState *src);

#endif /* PRACTICE_STATES_PROPS_H */
