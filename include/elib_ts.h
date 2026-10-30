/* ============================================================================
 * elib_ts.h --- elib task scheduler (elib-taskscheduler) unified umbrella header
 * ============================================================================
 * The complete public API and public types of this library live in this single
 * header. All sub-headers were removed as of v2.
 * Version: ELIB_TS_VERSION (currently 2.0.0)
 *
 * Game rules (elib family v2 common principles):
 *   1. C99; must compile warning-free with
 *      arm-gcc -std=c99 -Wall -Wextra -Werror -Iinclude.
 *   2. Zero dynamic memory, zero globals: the ctx and the task array are
 *      provided statically by the user.
 *   3. init memset-overwrites and re-initializes unconditionally; a ctx in any
 *      historical state is accepted; illegal configurations (NULL /
 *      max_tasks == 0 / a one-sided critical-hook pair) are rejected by init.
 *   4. Sources include this umbrella header directly: #include "elib_ts.h",
 *      built with -Iinclude; relative-path cross-layer includes are forbidden.
 *   5. No build system ships with the library: the only compile units are the
 *      source files plus this header.
 *
 * Time model:
 *   The tick side (elib_ts_tick_update, usually called from a 1 ms SysTick
 *   ISR) only accumulates elapsed milliseconds per task. The main loop side
 *   (elib_ts_poll) dispatches the tasks whose elapsed time reached their period.
 *   Nothing is executed from the ISR.
 *
 *   Lost-tick semantics: one poll pass runs a due task at most once and discards
 *   any surplus time beyond the period; there is no catch-up. Keep the tick
 *   granularity far below the smallest period.
 *
 *   While the scheduler is stopped, tick_update does not accumulate anything
 *   (stop freezes all schedules). elib_ts_start clears elapsed_ms of active
 *   tasks so every task restarts its period from a known state.
 *
 * Execution time and timeout:
 *   exec_time_ms is measured in tick units: ticks arriving while exec_cb runs
 *   are accumulated by tick_update (the task is in ELIB_TS_TASK_RUNNING during
 *   callback execution). Consequently the resolution equals the tick period,
 *   and an execution shorter than one tick reports 0.
 *   A timeout anomaly fires when timeout_ms > 0 and exec_time_ms >= timeout_ms.
 *
 * Priority:
 *   The array index is the dispatch order: index 0 dispatches first within one
 *   poll pass. Dispatch is strictly non-preemptive; there is no starvation
 *   guarantee beyond the index order.
 *
 * Callback contract:
 *   Callbacks run in the caller context of elib_ts_poll (the main loop). From a
 *   callback only operations on that task itself are allowed:
 *   task_stop / task_start / task_set_period / task_get_info on self.
 *   Registering or deleting other tasks and re-entering elib_ts_poll are
 *   undefined behaviour. After a callback returns, poll re-checks the task state
 *   before writing bookkeeping fields, so a task that stopped or deleted itself
 *   is not resurrected by the scheduler.
 *
 * Concurrency model:
 *   tick_update runs in the ISR, all other APIs run in the main loop. Fields
 *   shared across contexts (elapsed_ms, state) are volatile; the library
 *   assumes 32-bit-aligned loads/stores are atomic. The optional critical-hook
 *   pair passed to init (enter_critical / exit_critical) fences the
 *   scheduler-side state transitions; init rejects a one-sided pair.
 * ==========================================================================*/
#ifndef ELIB_TS_H
#define ELIB_TS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------- version */
#define ELIB_TS_VERSION        "2.0.0"
#define ELIB_TS_VERSION_MAJOR  2
#define ELIB_TS_VERSION_MINOR  0
#define ELIB_TS_VERSION_PATCH  0

/* --------------------------------------------------------------- error codes */
typedef enum {
    ELIB_TS_OK = 0,                    /* Success */
    ELIB_TS_ERR_INVALID_PARAM,         /* Illegal parameter */
    ELIB_TS_ERR_NOT_INITIALIZED,       /* Scheduler not initialized (or deinitialized) */
    ELIB_TS_ERR_NOT_FOUND,             /* Invalid task ID or unused slot */
    ELIB_TS_ERR_FULL,                  /* Task pool is full */
    ELIB_TS_ERR_NOT_RUNNING,           /* Scheduler is not running */
    ELIB_TS_ERR_ALREADY_RUNNING,       /* Scheduler is already running */
    ELIB_TS_ERR_TASK_STOPPED,          /* Task is already stopped */
    ELIB_TS_ERR_TASK_RUNNING,          /* Task is not in STOPPED state */
} elib_ts_err_t;

/* ------------------------------------------------------------------- types */
/* Task state machine: UNUSED -(register)-> READY / INIT (INIT when init_cb
 * exists) -> READY; READY/INIT -(task_stop)-> STOPPED -(task_start)-> back;
 * READY -(dispatch)-> RUNNING -(callback returns)-> READY */
typedef enum {
    ELIB_TS_TASK_UNUSED = 0,           /* Slot is free */
    ELIB_TS_TASK_STOPPED,              /* Registered but stopped */
    ELIB_TS_TASK_INIT,                 /* init_cb pending (runs in the next poll) */
    ELIB_TS_TASK_READY,                /* Waiting for its period to elapse */
    ELIB_TS_TASK_RUNNING,              /* exec_cb currently executing (transient) */
} elib_ts_task_state_t;

/* Anomaly type */
typedef enum {
    ELIB_TS_ANOMALY_TIMEOUT,           /* exec_time_ms reached timeout_ms */
} elib_ts_anomaly_t;

/* Task execution callback */
typedef void (*elib_ts_exec_cb_t)(void *user_data);

/* Task initialization callback (runs once in the next poll after register/start) */
typedef void (*elib_ts_init_cb_t)(void *user_data);

/* Anomaly callback */
typedef void (*elib_ts_anomaly_cb_t)(elib_ts_anomaly_t type, void *user_data);

/* Task instance (one element of the user's static array) */
typedef struct {
    /* User configuration */
    elib_ts_init_cb_t init_cb;         /* Optional, may be NULL */
    elib_ts_exec_cb_t exec_cb;         /* Mandatory */
    elib_ts_anomaly_cb_t anomaly_cb;   /* Optional, may be NULL */
    void *user_data;                   /* Opaque pointer passed to all callbacks */

    /* Runtime parameters (millisecond domain) */
    uint32_t period_ms;                /* Dispatch period */
    uint32_t timeout_ms;               /* Execution-time limit, 0 = disabled */
    volatile uint32_t elapsed_ms;      /* ISR-written time accumulator */
    uint32_t exec_time_ms;             /* Last execution time (tick-quantized) */
    uint32_t max_exec_time_ms;         /* Longest observed execution time */
    volatile elib_ts_task_state_t state; /* ISR-read task state */
} elib_ts_task_t;

/* Scheduler context (statically allocated by the user, initialized by init) */
typedef struct {
    /* Filled in by init */
    elib_ts_task_t *tasks;             /* Task array base pointer */
    uint32_t max_tasks;                /* Array capacity */
    void (*enter_critical)(void);      /* Optional critical-section hooks; */
    void (*exit_critical)(void);       /* both NULL or both non-NULL */

    /* Runtime state */
    volatile uint8_t running;          /* Scheduler running flag */
    volatile uint8_t initialized;      /* init/deinit flag */
} elib_ts_ctx_t;

/* Task info (query output, snapshot) */
typedef struct {
    uint32_t period_ms;
    uint32_t timeout_ms;
    uint32_t exec_time_ms;
    uint32_t max_exec_time_ms;
    elib_ts_task_state_t state;
} elib_ts_task_info_t;

/* --------------------------------------------------------------- lifecycle */
/**
 * @brief Initialize the scheduler
 * @param ctx User-provided context pointer (any historical state; memset-overwritten)
 * @param tasks User-provided task array
 * @param max_tasks Array capacity (must be >= 1)
 * @param enter_critical Optional critical-section entry hook (pair with exit_critical)
 * @param exit_critical Optional critical-section exit hook (pair with enter_critical)
 * @return OK / INVALID_PARAM (NULL, zero capacity, or one-sided hook pair)
 */
elib_ts_err_t elib_ts_init(elib_ts_ctx_t *ctx,
                           elib_ts_task_t *tasks,
                           uint32_t max_tasks,
                           void (*enter_critical)(void),
                           void (*exit_critical)(void));

/**
 * @brief Deinitialize the scheduler (NULL-safe; tasks are kept in place)
 */
void elib_ts_deinit(elib_ts_ctx_t *ctx);

/* ------------------------------------------------------- scheduler control */
/**
 * @brief Start the scheduler (clears elapsed_ms of all active tasks)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / ALREADY_RUNNING
 */
elib_ts_err_t elib_ts_start(elib_ts_ctx_t *ctx);

/**
 * @brief Stop the scheduler (freezes tick accumulation; poll becomes a no-op)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_RUNNING
 */
elib_ts_err_t elib_ts_stop(elib_ts_ctx_t *ctx);

/* Tick driver (ISR) */
/**
 * @brief Accumulate elapsed milliseconds (call from the tick ISR)
 *
 * No-op when ctx is NULL, not initialized, ms == 0 or the scheduler is
 * stopped. Accumulation covers READY / INIT / RUNNING tasks and saturates at
 * UINT32_MAX.
 */
void elib_ts_tick_update(elib_ts_ctx_t *ctx, uint32_t ms);

/* ------------------------------------------------------------- task control */
/**
 * @brief Register a task (first UNUSED slot; ID = array index)
 * @param ctx Context
 * @param period_ms Dispatch period in milliseconds (must be >= 1)
 * @param timeout_ms Execution-time limit in milliseconds (0 = disabled)
 * @param exec_cb Mandatory execution callback
 * @param init_cb Optional initialization callback
 * @param anomaly_cb Optional anomaly callback
 * @param user_data Opaque pointer passed to every callback
 * @param out_task_id Output ID (must not be NULL)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / FULL
 */
elib_ts_err_t elib_ts_register(elib_ts_ctx_t *ctx,
                               uint32_t period_ms,
                               uint32_t timeout_ms,
                               elib_ts_exec_cb_t exec_cb,
                               elib_ts_init_cb_t init_cb,
                               elib_ts_anomaly_cb_t anomaly_cb,
                               void *user_data,
                               uint32_t *out_task_id);

/**
 * @brief Start a stopped task (elapsed reset; enters INIT when init_cb exists)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_FOUND / TASK_RUNNING
 */
elib_ts_err_t elib_ts_task_start(elib_ts_ctx_t *ctx, uint32_t task_id);

/**
 * @brief Stop a task (no more dispatching; state returns only via task_start)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_FOUND / TASK_STOPPED
 */
elib_ts_err_t elib_ts_task_stop(elib_ts_ctx_t *ctx, uint32_t task_id);

/**
 * @brief Delete a task (slot returns to UNUSED and becomes reusable)
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_FOUND
 */
elib_ts_err_t elib_ts_task_delete(elib_ts_ctx_t *ctx, uint32_t task_id);

/**
 * @brief Change the dispatch period at runtime
 *
 * elapsed_ms is kept as absolute millisecond progress; the new period applies
 * to the next dispatch decision. Allowed in every non-UNUSED state, including
 * RUNNING (i.e. from the task's own exec callback).
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_FOUND
 */
elib_ts_err_t elib_ts_task_set_period(elib_ts_ctx_t *ctx,
                                      uint32_t task_id,
                                      uint32_t period_ms);

/**
 * @brief Query a task snapshot
 * @return OK / INVALID_PARAM / NOT_INITIALIZED / NOT_FOUND
 */
elib_ts_err_t elib_ts_task_get_info(elib_ts_ctx_t *ctx,
                                    uint32_t task_id,
                                    elib_ts_task_info_t *out_info);

/* ------------------------------------------------------------------ driver */
/**
 * @brief Scheduler driver (call from the main loop)
 *
 * A no-op returning OK while the scheduler is stopped. Tasks are visited in
 * array order (index 0 = highest priority). Each due task runs at most once per
 * call; init_cb tasks run their init once and become READY in the same pass
 * without executing exec_cb.
 * @return OK / INVALID_PARAM / NOT_INITIALIZED
 */
elib_ts_err_t elib_ts_poll(elib_ts_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_TS_H */
