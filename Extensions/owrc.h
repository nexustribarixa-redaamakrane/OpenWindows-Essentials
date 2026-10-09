/*
 * owrc.h - OpenWindows Run Control (RC) Engine & Service Orchestrator
 *
 * Provides a Linux-like run control abstraction for freestanding OpenWindows:
 * runlevels (0-6), service status tracking, start/stop/restart/reload dispatch,
 * and parsing of /config/rc.conf and /config/rc.d/ scripts.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWRC_H
#define OWRC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Runlevel Definitions ─────────────────────────────────────── */

typedef enum {
    OWRC_RUNLEVEL_HALT       = 0, /* Clean system shutdown / poweroff   */
    OWRC_RUNLEVEL_SINGLE     = 1, /* Emergency / Sentinel recovery mode */
    OWRC_RUNLEVEL_MULTI      = 2, /* Multi-user text mode, no network   */
    OWRC_RUNLEVEL_NETWORK    = 3, /* Multi-user text mode with network  */
    OWRC_RUNLEVEL_RESERVED   = 4, /* User-defined runlevel              */
    OWRC_RUNLEVEL_GUI        = 5, /* Graphical desktop (Cairo64 + owwm) */
    OWRC_RUNLEVEL_REBOOT     = 6  /* Clean reboot                       */
} owrc_runlevel_t;

/* ── Service Command Verbs ────────────────────────────────────── */

typedef enum {
    OWRC_CMD_START           = 0,
    OWRC_CMD_STOP            = 1,
    OWRC_CMD_RESTART         = 2,
    OWRC_CMD_RELOAD          = 3,
    OWRC_CMD_STATUS          = 4
} owrc_cmd_t;

/* ── Service State ────────────────────────────────────────────── */

typedef enum {
    OWRC_STATUS_STOPPED      = 0,
    OWRC_STATUS_STARTING     = 1,
    OWRC_STATUS_RUNNING      = 2,
    OWRC_STATUS_STOPPING     = 3,
    OWRC_STATUS_FAILED       = 4
} owrc_status_t;

/* ── Maximum limits (Zero-allocation) ─────────────────────────── */

#define OWRC_MAX_SERVICES    32u
#define OWRC_NAME_MAX        32u
#define OWRC_PATH_MAX        128u

typedef struct {
    char          name[OWRC_NAME_MAX];
    char          script_path[OWRC_PATH_MAX];
    uint32_t      pid;
    owrc_status_t status;
    uint32_t      start_order;
    uint32_t      restart_count;
    bool          auto_restart;
} owrc_service_entry_t;

typedef struct {
    owrc_runlevel_t      current_runlevel;
    owrc_runlevel_t      previous_runlevel;
    uint32_t             service_count;
    owrc_service_entry_t services[OWRC_MAX_SERVICES];
    bool                 initialized;
} owrc_manager_t;

/* ── Public API Prototypes ────────────────────────────────────── */

void owrc_init(owrc_manager_t *mgr);
bool owrc_register_service(owrc_manager_t *mgr, const char *name, const char *script_path, uint32_t order);
bool owrc_dispatch(owrc_manager_t *mgr, const char *service_name, owrc_cmd_t cmd);
bool owrc_switch_runlevel(owrc_manager_t *mgr, owrc_runlevel_t target);
owrc_status_t owrc_get_service_status(const owrc_manager_t *mgr, const char *service_name);
bool owrc_parse_rc_conf(owrc_manager_t *mgr, const char *conf_buffer, size_t buffer_len);

#ifdef __cplusplus
}
#endif

#endif /* OWRC_H */
