#ifndef PRACTICE_DEBUG_H
#define PRACTICE_DEBUG_H

#define STATE_DOOR 1
#define STATE_PICKUP 2

/**
 * Log memory usage stats for all memory pools via practiceLogInfo.
 * Reports MEMPOOL_STAGE, MEMPOOL_PERMANENT, total pool area, and overall
 * 4MB RDRAM breakdown.
 */
void practice_log_memory_usage();

void practice_debug_tick();
void practice_briefing_menu_tick(void);

#endif /* PRACTICE_DEBUG_H */
