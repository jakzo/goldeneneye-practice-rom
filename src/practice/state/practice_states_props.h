#ifndef PRACTICE_STATES_PROPS_H
#define PRACTICE_STATES_PROPS_H

#include "practice_states_stream.h"
#include <bondtypes.h>
#include <ultra64.h>

// Enable only after every prop type has allocation and teardown support.
#define ADD_AND_REMOVE_PROPS FALSE

bool save_props_state(StateStream *stream);
bool load_props_state(StateStream *stream);

#endif /* PRACTICE_STATES_PROPS_H */
