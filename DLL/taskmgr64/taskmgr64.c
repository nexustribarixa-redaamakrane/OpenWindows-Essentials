/*
 * taskmgr64.c - OpenWindows Process Lifecycle Table Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "taskmgr64.h"

void taskmgr64_init(taskmgr_table_t *table)
{
    if (!table) return;
    table->active_count = 0u;
    table->next_pid = 1u;
    for (size_t i = 0u; i < TASKMGR_MAX_PROCESSES; ++i) {
        table->processes[i].pid = 0u;
        table->processes[i].state = PROC_STATE_INACTIVE;
    }
}

int32_t taskmgr64_spawn(taskmgr_table_t *table, const char *name, uint64_t cr3)
{
    if (!table) return -1;

    for (size_t i = 0u; i < TASKMGR_MAX_PROCESSES; ++i) {
        if (table->processes[i].pid == 0u) {
            uint32_t pid = table->next_pid++;
            table->processes[i].pid = pid;
            table->processes[i].state = PROC_STATE_ACTIVE;
            table->processes[i].cr3_page_root = cr3;
            table->processes[i].thread_count = 1u;
            table->processes[i].memory_consumed_bytes = 65536u;

            size_t ni = 0u;
            if (name) {
                while (ni < TASKMGR_NAME_MAX - 1 && name[ni]) {
                    table->processes[i].name[ni] = name[ni];
                    ni++;
                }
            }
            table->processes[i].name[ni] = '\0';

            table->active_count++;
            return (int32_t)pid;
        }
    }
    return -1;
}

bool taskmgr64_kill(taskmgr_table_t *table, uint32_t pid)
{
    if (!table || pid == 0u) return false;

    for (size_t i = 0u; i < TASKMGR_MAX_PROCESSES; ++i) {
        if (table->processes[i].pid == pid && table->processes[i].state != PROC_STATE_INACTIVE) {
            table->processes[i].state = PROC_STATE_INACTIVE;
            table->processes[i].pid = 0u;
            if (table->active_count > 0u) table->active_count--;
            return true;
        }
    }
    return false;
}

const task_process_entry_t *taskmgr64_find(const taskmgr_table_t *table, uint32_t pid)
{
    if (!table || pid == 0u) return NULL;

    for (size_t i = 0u; i < TASKMGR_MAX_PROCESSES; ++i) {
        if (table->processes[i].pid == pid && table->processes[i].state != PROC_STATE_INACTIVE) {
            return &table->processes[i];
        }
    }
    return NULL;
}
