/* test_timeout.c --- timeout anomaly: >= boundary / disable / null-cb / user_data */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count;
static uint32_t ticks_during_exec;
static uint32_t anomaly_count;
static elib_ts_anomaly_t last_type;
static void *last_ud;

static void exec_cb_ticking(void *ud) {
    (void)ud;
    exec_count++;
    for (uint32_t i = 0u; i < ticks_during_exec; i++) {
        elib_ts_tick_update(&ctx, 1u);
    }
}

static void anomaly_cb(elib_ts_anomaly_t type, void *ud) {
    anomaly_count++;
    last_type = type;
    last_ud = ud;
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    ticks_during_exec = 0u;
    anomaly_count = 0u;
    last_type = (elib_ts_anomaly_t)0;
    last_ud = NULL;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, uint32_t timeout,
                   elib_ts_anomaly_cb_t acb, void *ud) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, timeout, exec_cb_ticking,
                            NULL, acb, ud, &id) == ELIB_TS_OK);
    return id;
}

void test_timeout_fires(void) {
    printf("Test: exec_time above timeout raises anomaly... ");
    reset();
    ticks_during_exec = 3u;
    (void)mk(5u, 2u, anomaly_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 1u);
    assert(last_type == ELIB_TS_ANOMALY_TIMEOUT);
    printf("PASSED\n");
}

void test_timeout_boundary_equal(void) {
    printf("Test: exec_time == timeout raises anomaly (>=)... ");
    reset();
    ticks_during_exec = 3u;
    (void)mk(5u, 3u, anomaly_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 1u);
    printf("PASSED\n");
}

void test_timeout_below(void) {
    printf("Test: exec_time below timeout stays silent... ");
    reset();
    ticks_during_exec = 3u;
    (void)mk(5u, 4u, anomaly_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 0u);
    printf("PASSED\n");
}

void test_timeout_zero_disables(void) {
    printf("Test: timeout 0 disables anomaly detection... ");
    reset();
    ticks_during_exec = 3u;
    (void)mk(5u, 0u, anomaly_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 0u);
    printf("PASSED\n");
}

void test_timeout_null_cb_safe(void) {
    printf("Test: NULL anomaly callback is harmless... ");
    reset();
    ticks_during_exec = 3u;
    uint32_t id = mk(5u, 2u, NULL, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 3u);
    assert(anomaly_count == 0u);
    printf("PASSED\n");
}

void test_timeout_user_data_passed(void) {
    printf("Test: anomaly callback receives the task user_data... ");
    reset();
    int marker = 0;
    ticks_during_exec = 3u;
    (void)mk(5u, 2u, anomaly_cb, &marker);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 1u);
    assert(last_ud == &marker);
    printf("PASSED\n");
}

void test_timeout_per_run(void) {
    printf("Test: anomaly fires once per overrun execution... ");
    reset();
    ticks_during_exec = 3u;
    (void)mk(5u, 2u, anomaly_cb, NULL);
    (void)elib_ts_start(&ctx);

    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 1u);

    elib_ts_tick_update(&ctx, 2u);             /* elapsed 3 + 2 = 5 */
    (void)elib_ts_poll(&ctx);
    assert(anomaly_count == 2u);
    assert(exec_count == 2u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_timeout ===\n");
    test_timeout_fires();
    test_timeout_boundary_equal();
    test_timeout_below();
    test_timeout_zero_disables();
    test_timeout_null_cb_safe();
    test_timeout_user_data_passed();
    test_timeout_per_run();
    printf("=== test_timeout: all PASSED ===\n");
    return 0;
}
