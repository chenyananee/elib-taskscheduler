/* elib_ts_types.h - Task Scheduler Type Definitions */
#ifndef ELIB_TS_TYPES_H
#define ELIB_TS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "elib_ts_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Task state */
typedef enum {
    ELIB_TS_TASK_UNUSED = 0,
    ELIB_TS_TASK_STOPPED,
    ELIB_TS_TASK_INIT,
    ELIB_TS_TASK_READY,
    ELIB_TS_TASK_RUNNING,
} elib_ts_task_state_t;

/* Anomaly type */
typedef enum {
    ELIB_TS_ANOMALY_TIMEOUT,
} elib_ts_anomaly_t;

/* Task instance */
typedef struct {
    void (*init_cb)(void);
    void (*exec_cb)(void);
    void (*anomaly_cb)(elib_ts_anomaly_t type);
    uint32_t period_ms;
    uint32_t timeout_ms;
    uint32_t elapsed_ms;
    uint32_t exec_time_ms;
    uint32_t max_exec_time_ms;
    elib_ts_task_state_t state;
} elib_ts_task_t;

/* Scheduler context */
typedef struct {
    elib_ts_task_t *tasks;
    uint32_t max_tasks;
    bool running;
    void (*enter_critical)(void);
    void (*exit_critical)(void);
    int initialized;
} elib_ts_ctx_t;

/* Task info (query output) */
typedef struct {
    uint32_t period_ms;
    uint32_t timeout_ms;
    uint32_t max_exec_time_ms;
    elib_ts_task_state_t state;
} elib_ts_task_info_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_TS_TYPES_H */
