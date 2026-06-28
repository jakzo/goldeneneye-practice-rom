#ifndef PRACTICE_STATES_CHR_H
#define PRACTICE_STATES_CHR_H

#include "practice_states_stream.h"
#include <bondtypes.h>

void save_chr_record(StateStream *stream, const ChrRecord *chr);
void load_chr_record(StateStream *stream, ChrRecord *chr);

#endif /* PRACTICE_STATES_CHR_H */
