/* elib_ts.h - Task Scheduler Main Header */
#ifndef ELIB_TS_H
#define ELIB_TS_H

#include "elib_ts_err.h"
#include "elib_ts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init / Deinit */
elib_ts_err_t elib_ts_init(elib_ts_ctx_t *ctx,
                            elib_ts_task_t *tasks,
                            uint32_t max_tasks,
                            void (*enter_critical)(void),
                            void (*exit_critical)(void));
void elib_ts_deinit(elib_ts_ctx_t *ctx);

/* Scheduler Start / Stop */
elib_ts_err_t elib_ts_start(elib_ts_ctx_t *ctx);
elib_ts_err_t elib_ts_stop(elib_ts_ctx_t *ctx);

/* Tick Driver (ISR) */
void elib_ts_tick_update(elib_ts_ctx_t *ctx, uint32_t ms);

/* Task Register */
elib_ts_err_t elib_ts_register(elib_ts_ctx_t *ctx,
                                uint32_t period_ms,
                                uint32_t timeout_ms,
                                void (*exec_cb)(void),
                                void (*init_cb)(void),
                                void (*anomaly_cb)(elib_ts_anomaly_t),
                                uint32_t *out_task_id);

/* Task Start / Stop */
elib_ts_err_t elib_ts_task_start(elib_ts_ctx_t *ctx, uint32_t task_id);
elib_ts_err_t elib_ts_task_stop(elib_ts_ctx_t *ctx, uint32_t task_id);

/* Task Set Period */
elib_ts_err_t elib_ts_task_set_period(elib_ts_ctx_t *ctx, uint32_t task_id,
                                       uint32_t period_ms);

/* Task Get Info */
elib_ts_err_t elib_ts_task_get_info(elib_ts_ctx_t *ctx, uint32_t task_id,
                                     elib_ts_task_info_t *out_info);

/* Poll (Main Loop) */
elib_ts_err_t elib_ts_poll(elib_ts_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_TS_H */
