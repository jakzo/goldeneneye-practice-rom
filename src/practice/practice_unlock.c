#include <ultra64.h>
#include "bondconstants.h"
#include "game/file.h"
#include "game/file2.h"
#include "practice_unlock.h"

void practice_unlock_default_profile(void) {
    save_data *save;
    bool isUnlocked;
    s32 level;
    s32 diff;

    save = fileGetSaveForFoldernum(FOLDER1);
    
    if (!save) {
        fileBuildWriteNewSave(FOLDER1);
        save = fileGetSaveForFoldernum(FOLDER1);
    }
    if (!save) return;
    
    isUnlocked =
        save->unlocked_cheats_1 == 0xFF &&
        save->unlocked_cheats_2 == 0xFF &&
        save->unlocked_cheats_3 == 0xFF &&
        (save->flag_007 & 1) != 0;
    if (isUnlocked) return;

    for (level = SP_LEVEL_DAM; level < SP_LEVEL_MAX; level++) {
        for (diff = DIFFICULTY_AGENT; diff < DIFFICULTY_007; diff++) {
            fileSetDifficultyStageTime(save, level, diff, 0x3FF);
        }
    }
    
    save->unlocked_cheats_1 = 0xFF;
    save->unlocked_cheats_2 = 0xFF;
    save->unlocked_cheats_3 = 0xFF;
    save->flag_007 |= 1;
    
    fileWriteSave(save);
}
