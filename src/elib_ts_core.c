/* ============================================================================
 * elib_ts_core.c --- elib task scheduler implementation (v2)
 * ============================================================================
 * The single source file; includes the umbrella header directly. The semantic
 * contract lives in the header banner of elib_ts.h:
 *   - ISR side only accumulates elapsed_ms; the main loop (poll) dispatches.
 *   - One due task runs at most once per poll pass (lost-tick, no catch-up).
 *   - tick_update is frozen while the scheduler is stopped; elib_ts_start
 *     clears elapsed_ms of active tasks.
 *   - exec_time_ms is measured in tick units (ticks during exec_cb), and a
 *     timeout anomaly fires on exec_time_ms >= timeout_ms (when enabled).
 *   - After a callback returns, poll re-checks the task state: a task that
 *     stopped or deleted itself from its own callback is not resurrected.
 * ==========================================================================*/
#include "elib_ts.h"
#include <string.h>

/* -------------------------------------------------------------- internal helpers */

/* Fetch the task by index (ID = array index); NULL when out of range */
static elib_ts_task_t *get_task_by_id(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (task_id >= ctx->max_tasks) {
        return NULL;
    }
    return &ctx->tasks[task_id];
}

/* Enter the optional user critical section (init guarantees a matched pair) */
static void ts_lock(elib_ts_ctx_t *ctx) {
    if (ctx->enter_critical != NULL) {
        ctx->enter_critical();
    }
}

/* Leave the optional user critical section */
static void ts_unlock(elib_ts_ctx_t *ctx) {
    if (ctx->exit_critical != NULL) {
        ctx->exit_critical();
    }
}

/* -------------------------------------------------------------- lifecycle */

elib_ts_err_t elib_ts_init(elib_ts_ctx_t *ctx,
                           elib_ts_task_t *tasks,
                           uint32_t max_tasks,
                           void (*enter_critical)(void),
                           void (*exit_critical)(void)) {
    if (ctx == NULL || tasks == NULL || max_tasks == 0u) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    /* A one-sided hook pair would leave the interrupt state inconsistent */
    if ((enter_critical == NULL) != (exit_critical == NULL)) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    memset(tasks, 0, sizeof(elib_ts_task_t) * max_tasks);

    ctx->tasks = tasks;
    ctx->max_tasks = max_tasks;
    ctx->enter_critical = enter_critical;
    ctx->exit_critical = exit_critical;
    ctx->running = 0u;
    ctx->initialized = 1u;

    return ELIB_TS_OK;
}

void elib_ts_deinit(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    ctx->running = 0u;
    ctx->initialized = 0u;
}

/* ------------------------------------------------------- scheduler control */

elib_ts_err_t elib_ts_start(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (ctx->running) {
        return ELIB_TS_ERR_ALREADY_RUNNING;
    }

    ts_lock(ctx);
    ctx->running = 1u;
    /* Restart every active schedule from a known state */
    for (uint32_t i = 0u; i < ctx->max_tasks; i++) {
        elib_ts_task_state_t state = ctx->tasks[i].state;
        if (state == ELIB_TS_TASK_INIT || state == ELIB_TS_TASK_READY) {
            ctx->tasks[i].elapsed_ms = 0u;
        }
    }
    ts_unlock(ctx);

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_stop(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (!ctx->running) {
        return ELIB_TS_ERR_NOT_RUNNING;
    }

    ts_lock(ctx);
    ctx->running = 0u;
    ts_unlock(ctx);

    return ELIB_TS_OK;
}

/* Tick driver (ISR side): accumulate only, never execute anything */
void elib_ts_tick_update(elib_ts_ctx_t *ctx, uint32_t ms) {
    if (ctx == NULL || !ctx->initialized || ms == 0u || !ctx->running) {
        return;
    }

    for (uint32_t i = 0u; i < ctx->max_tasks; i++) {
        elib_ts_task_t *task = &ctx->tasks[i];
        elib_ts_task_state_t state = task->state;

        /* RUNNING is included: ticks during exec_cb are the execution time */
        if (state == ELIB_TS_TASK_READY ||
            state == ELIB_TS_TASK_INIT ||
            state == ELIB_TS_TASK_RUNNING) {
            uint32_t current = task->elapsed_ms;
            uint32_t next = current + ms;
            task->elapsed_ms = (next < current) ? UINT32_MAX : next;
        }
    }
}

/* ------------------------------------------------------------- task control */

elib_ts_err_t elib_ts_register(elib_ts_ctx_t *ctx,
                               uint32_t period_ms,
                               uint32_t timeout_ms,
                               elib_ts_exec_cb_t exec_cb,
                               elib_ts_init_cb_t init_cb,
                               elib_ts_anomaly_cb_t anomaly_cb,
                               void *user_data,
                               uint32_t *out_task_id) {
    if (ctx == NULL || exec_cb == NULL || out_task_id == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (period_ms == 0u) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    /* Find the first UNUSED slot (ID = index) */
    uint32_t task_id = ctx->max_tasks;
    for (uint32_t i = 0u; i < ctx->max_tasks; i++) {
        if (ctx->tasks[i].state == ELIB_TS_TASK_UNUSED) {
            task_id = i;
            break;
        }
    }
    if (task_id == ctx->max_tasks) {
        return ELIB_TS_ERR_FULL;
    }

    elib_ts_task_t *task = &ctx->tasks[task_id];

    /* memset first: the ISR observes UNUSED until the state is written last */
    memset(task, 0, sizeof(*task));
    task->init_cb = init_cb;
    task->exec_cb = exec_cb;
    task->anomaly_cb = anomaly_cb;
    task->user_data = user_data;
    task->period_ms = period_ms;
    task->timeout_ms = timeout_ms;
    task->elapsed_ms = 0u;
    task->exec_time_ms = 0u;
    task->max_exec_time_ms = 0u;
    task->state = (init_cb != NULL) ? ELIB_TS_TASK_INIT : ELIB_TS_TASK_READY;

    *out_task_id = task_id;

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_task_start(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }
    if (task->state != ELIB_TS_TASK_STOPPED) {
        return ELIB_TS_ERR_TASK_RUNNING;
    }

    ts_lock(ctx);
    task->elapsed_ms = 0u;
    task->state = (task->init_cb != NULL) ? ELIB_TS_TASK_INIT
                                          : ELIB_TS_TASK_READY;
    ts_unlock(ctx);

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_task_stop(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }
    if (task->state == ELIB_TS_TASK_STOPPED) {
        return ELIB_TS_ERR_TASK_STOPPED;
    }

    ts_lock(ctx);
    task->state = ELIB_TS_TASK_STOPPED;
    ts_unlock(ctx);

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_task_delete(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }

    ts_lock(ctx);
    memset(task, 0, sizeof(*task));   /* state returns to UNUSED, slot reusable */
    ts_unlock(ctx);

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_task_set_period(elib_ts_ctx_t *ctx,
                                      uint32_t task_id,
                                      uint32_t period_ms) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (period_ms == 0u) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }

    /* Allowed in every non-UNUSED state, including RUNNING (self call) */
    task->period_ms = period_ms;

    return ELIB_TS_OK;
}

elib_ts_err_t elib_ts_task_get_info(elib_ts_ctx_t *ctx,
                                    uint32_t task_id,
                                    elib_ts_task_info_t *out_info) {
    if (ctx == NULL || out_info == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }

    out_info->period_ms = task->period_ms;
    out_info->timeout_ms = task->timeout_ms;
    out_info->exec_time_ms = task->exec_time_ms;
    out_info->max_exec_time_ms = task->max_exec_time_ms;
    out_info->state = task->state;

    return ELIB_TS_OK;
}

/* ------------------------------------------------------------------ driver */

elib_ts_err_t elib_ts_poll(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (!ctx->running) {
        return ELIB_TS_OK;
    }

    for (uint32_t i = 0u; i < ctx->max_tasks; i++) {
        elib_ts_task_t *task = &ctx->tasks[i];

        switch (task->state) {
        case ELIB_TS_TASK_UNUSED:
        case ELIB_TS_TASK_STOPPED:
            continue;

        case ELIB_TS_TASK_INIT:
            /* init_cb runs outside the critical section */
            if (task->init_cb != NULL) {
                task->init_cb(task->user_data);
            }
            /* The callback may have stopped or deleted this very task */
            if (task->state == ELIB_TS_TASK_INIT) {
                ts_lock(ctx);
                task->state = ELIB_TS_TASK_READY;
                task->elapsed_ms = 0u;
                ts_unlock(ctx);
            }
            continue;

        case ELIB_TS_TASK_READY:
        case ELIB_TS_TASK_RUNNING:
            if (task->elapsed_ms < task->period_ms) {
                continue;
            }

            /* Mark RUNNING and reset the accumulator atomically w.r.t. the ISR:
             * ticks arriving during exec_cb are counted as execution time */
            ts_lock(ctx);
            task->state = ELIB_TS_TASK_RUNNING;
            task->elapsed_ms = 0u;
            ts_unlock(ctx);

            task->exec_cb(task->user_data);

            /* A self-delete removed the task: no more bookkeeping */
            if (task->state == ELIB_TS_TASK_UNUSED) {
                continue;
            }

            task->exec_time_ms = task->elapsed_ms;

            if (task->timeout_ms > 0u &&
                task->exec_time_ms >= task->timeout_ms &&
                task->anomaly_cb != NULL) {
                task->anomaly_cb(ELIB_TS_ANOMALY_TIMEOUT, task->user_data);
            }

            /* The anomaly callback may have deleted the task as well */
            if (task->state == ELIB_TS_TASK_UNUSED) {
                continue;
            }

            if (task->exec_time_ms > task->max_exec_time_ms) {
                task->max_exec_time_ms = task->exec_time_ms;
            }

            /* Only a still-RUNNING task returns to READY; a task stopped by its
             * own callback stays STOPPED */
            if (task->state == ELIB_TS_TASK_RUNNING) {
                ts_lock(ctx);
                if (task->state == ELIB_TS_TASK_RUNNING) {
                    task->state = ELIB_TS_TASK_READY;
                }
                ts_unlock(ctx);
            }
            break;

        default:
            /* Corrupted state: ignore defensively */
            continue;
        }
    }

    return ELIB_TS_OK;
}
