/* elib_ts_core.c - Task Scheduler Core Implementation */
#include "../include/elib_ts.h"
#include <string.h>

/* Forward declarations */
static elib_ts_task_t* find_free_task(elib_ts_ctx_t *ctx);
static elib_ts_task_t* get_task_by_id(elib_ts_ctx_t *ctx, uint32_t task_id);

/* Find a free task slot */
static elib_ts_task_t* find_free_task(elib_ts_ctx_t *ctx) {
    for (uint32_t i = 0; i < ctx->max_tasks; i++) {
        if (ctx->tasks[i].state == ELIB_TS_TASK_UNUSED) {
            return &ctx->tasks[i];
        }
    }
    return NULL;
}

/* Get task by ID (array index) */
static elib_ts_task_t* get_task_by_id(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (task_id >= ctx->max_tasks) {
        return NULL;
    }
    return &ctx->tasks[task_id];
}

/* Initialize scheduler */
elib_ts_err_t elib_ts_init(elib_ts_ctx_t *ctx,
                            elib_ts_task_t *tasks,
                            uint32_t max_tasks,
                            void (*enter_critical)(void),
                            void (*exit_critical)(void)) {
    if (ctx == NULL || tasks == NULL || max_tasks == 0) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(elib_ts_ctx_t));
    memset(tasks, 0, sizeof(elib_ts_task_t) * max_tasks);

    ctx->tasks = tasks;
    ctx->max_tasks = max_tasks;
    ctx->bit_flags.running = 0;
    ctx->enter_critical = enter_critical;
    ctx->exit_critical = exit_critical;
    ctx->bit_flags.initialized = 1;

    return ELIB_TS_OK;
}

/* Deinitialize scheduler */
void elib_ts_deinit(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->bit_flags.initialized = 0;
}

/* Start scheduler */
elib_ts_err_t elib_ts_start(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (ctx->bit_flags.running) {
        return ELIB_TS_ERR_ALREADY_RUNNING;
    }

    ctx->bit_flags.running = 1;
    return ELIB_TS_OK;
}

/* Stop scheduler */
elib_ts_err_t elib_ts_stop(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (!ctx->bit_flags.running) {
        return ELIB_TS_ERR_NOT_RUNNING;
    }

    ctx->bit_flags.running = 0;
    return ELIB_TS_OK;
}

/* Tick update - call from ISR */
void elib_ts_tick_update(elib_ts_ctx_t *ctx, uint32_t ms) {
    if (ctx == NULL || !ctx->bit_flags.initialized || ms == 0) {
        return;
    }

    for (uint32_t i = 0; i < ctx->max_tasks; i++) {
        elib_ts_task_t *task = &ctx->tasks[i];
        if (task->state == ELIB_TS_TASK_READY || task->state == ELIB_TS_TASK_INIT) {
            uint32_t new_elapsed = task->elapsed_ms + ms;
            if (new_elapsed < task->elapsed_ms) {
                task->elapsed_ms = UINT32_MAX;
            } else {
                task->elapsed_ms = new_elapsed;
            }
        }
    }
}

/* Register a new task */
elib_ts_err_t elib_ts_register(elib_ts_ctx_t *ctx,
                                uint32_t period_ms,
                                uint32_t timeout_ms,
                                void (*exec_cb)(void),
                                void (*init_cb)(void),
                                void (*anomaly_cb)(elib_ts_anomaly_t),
                                uint32_t *out_task_id) {
    if (ctx == NULL || exec_cb == NULL || out_task_id == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (period_ms == 0) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    elib_ts_task_t *task = find_free_task(ctx);
    if (task == NULL) {
        return ELIB_TS_ERR_FULL;
    }

    uint32_t task_id = (uint32_t)(task - ctx->tasks);

    memset(task, 0, sizeof(elib_ts_task_t));
    task->init_cb = init_cb;
    task->exec_cb = exec_cb;
    task->anomaly_cb = anomaly_cb;
    task->period_ms = period_ms;
    task->timeout_ms = timeout_ms;
    task->elapsed_ms = 0;
    task->exec_time_ms = 0;
    task->max_exec_time_ms = 0;
    task->state = (init_cb != NULL) ? ELIB_TS_TASK_INIT : ELIB_TS_TASK_READY;

    *out_task_id = task_id;

    return ELIB_TS_OK;
}

/* Start a task */
elib_ts_err_t elib_ts_task_start(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }
    if (task->state != ELIB_TS_TASK_STOPPED) {
        return ELIB_TS_ERR_TASK_RUNNING;
    }

    task->elapsed_ms = 0;
    task->state = (task->init_cb != NULL) ? ELIB_TS_TASK_INIT : ELIB_TS_TASK_READY;
    return ELIB_TS_OK;
}

/* Stop a task */
elib_ts_err_t elib_ts_task_stop(elib_ts_ctx_t *ctx, uint32_t task_id) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }
    if (task->state == ELIB_TS_TASK_STOPPED) {
        return ELIB_TS_ERR_TASK_STOPPED;
    }

    task->state = ELIB_TS_TASK_STOPPED;
    return ELIB_TS_OK;
}

/* Set task period dynamically */
elib_ts_err_t elib_ts_task_set_period(elib_ts_ctx_t *ctx, uint32_t task_id,
                                       uint32_t period_ms) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (period_ms == 0) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }
    if (task->state == ELIB_TS_TASK_RUNNING) {
        return ELIB_TS_ERR_TASK_RUNNING;
    }

    task->period_ms = period_ms;
    return ELIB_TS_OK;
}

/* Get task info */
elib_ts_err_t elib_ts_task_get_info(elib_ts_ctx_t *ctx, uint32_t task_id,
                                     elib_ts_task_info_t *out_info) {
    if (ctx == NULL || out_info == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }

    elib_ts_task_t *task = get_task_by_id(ctx, task_id);
    if (task == NULL || task->state == ELIB_TS_TASK_UNUSED) {
        return ELIB_TS_ERR_NOT_FOUND;
    }

    out_info->period_ms = task->period_ms;
    out_info->timeout_ms = task->timeout_ms;
    out_info->max_exec_time_ms = task->max_exec_time_ms;
    out_info->state = task->state;

    return ELIB_TS_OK;
}

/* Poll - main loop scheduler driver */
elib_ts_err_t elib_ts_poll(elib_ts_ctx_t *ctx) {
    if (ctx == NULL) {
        return ELIB_TS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_TS_ERR_NOT_INITIALIZED;
    }
    if (!ctx->running) {
        return ELIB_TS_OK;
    }

    for (uint32_t i = 0; i < ctx->max_tasks; i++) {
        elib_ts_task_t *task = &ctx->tasks[i];

        switch (task->state) {
        case ELIB_TS_TASK_UNUSED:
        case ELIB_TS_TASK_STOPPED:
            continue;

        case ELIB_TS_TASK_INIT:
            if (task->init_cb) {
                task->init_cb();
            }
            task->state = ELIB_TS_TASK_READY;
            task->elapsed_ms = 0;
            continue;

        case ELIB_TS_TASK_READY:
            if (task->elapsed_ms < task->period_ms) {
                continue;
            }
            task->state = ELIB_TS_TASK_RUNNING;
            if (ctx->enter_critical) {
                ctx->enter_critical();
            }
            task->elapsed_ms = 0;
            if (ctx->exit_critical) {
                ctx->exit_critical();
            }
            task->exec_cb();
            task->exec_time_ms = task->elapsed_ms;
            if (task->timeout_ms > 0 && task->exec_time_ms > task->timeout_ms) {
                if (task->anomaly_cb) {
                    task->anomaly_cb(ELIB_TS_ANOMALY_TIMEOUT);
                }
            }
            if (task->exec_time_ms > task->max_exec_time_ms) {
                task->max_exec_time_ms = task->exec_time_ms;
            }
            task->state = ELIB_TS_TASK_READY;
            break;
        }
    }

    return ELIB_TS_OK;
}
