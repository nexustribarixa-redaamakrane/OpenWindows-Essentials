/*
 * taskmgr64.h - OpenWindows Process Lifecycle Table Dynamic Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef TASKMGR64_H
#define TASKMGR64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TASKMGR_MAX_PROCESSES   32u
#define TASKMGR_NAME_MAX        32u

typedef enum {
    PROC_STATE_INACTIVE = 0,
    PROC_STATE_ACTIVE   = 1,
    PROC_STATE_FROZEN   = 2,
    PROC_STATE_CRASHED  = 3
} proc_state_t;

typedef struct {
    uint32_t     pid;
    char         name[TASKMGR_NAME_MAX];
    proc_state_t state;
    uint64_t     cr3_page_root;
    uint32_t     thread_count;
    uint64_t     memory_consumed_bytes;
} task_process_entry_t;

typedef struct {
    task_process_entry_t processes[TASKMGR_MAX_PROCESSES];
    uint32_t             active_count;
    uint32_t             next_pid;
} taskmgr_table_t;

void taskmgr64_init(taskmgr_table_t *table);
int32_t taskmgr64_spawn(taskmgr_table_t *table, const char *name, uint64_t cr3);
bool taskmgr64_kill(taskmgr_table_t *table, uint32_t pid);
const task_process_entry_t *taskmgr64_find(const taskmgr_table_t *table, uint32_t pid);

#endif /* TASKMGR64_H */
