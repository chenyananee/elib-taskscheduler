/* test_get_info.c --- task info snapshot / parameter validation */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t ticks_during_exec;
static uint32_t exec_count;

static void exec_cb(void *ud) {
    (void)ud;
    exec_count++;
    for (uint32_t i = 0u; i < ticks_during_exec; i++) {
        elib_ts_tick_update(&ctx, 1u);
    }
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    ticks_during_exec = 0u;
    exec_count = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, uint32_t timeout) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, timeout, exec_cb, NULL, NULL,
                            NULL, &id) == ELIB_TS_OK);
    return id;
}

void test_info_initial(void) {
    printf("Test: fresh task info snapshot... ");
    reset();
    uint32_t id = mk(100u, 50u);
    elib_ts_task_info_t info;
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_OK);
    assert(info.period_ms == 100u);
    assert(info.timeout_ms == 50u);
    assert(info.exec_time_ms == 0u);
    assert(info.max_exec_time_ms == 0u);
    assert(info.state == ELIB_TS_TASK_READY);
    printf("PASSED\n");
}

void test_info_after_run(void) {
    printf("Test: info reflects a completed execution... ");
    reset();
    ticks_during_exec = 2u;
    uint32_t id = mk(5u, 10u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    elib_ts_task_info_t info;
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_OK);
    assert(info.exec_time_ms == 2u);
    assert(info.max_exec_time_ms == 2u);
    assert(info.state == ELIB_TS_TASK_READY);
    printf("PASSED\n");
}

void test_info_state_stopped(void) {
    printf("Test: info reports STOPPED after task_stop... ");
    reset();
    uint32_t id = mk(100u, 0u);
    (void)elib_ts_task_stop(&ctx, id);
    elib_ts_task_info_t info;
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_OK);
    assert(info.state == ELIB_TS_TASK_STOPPED);
    printf("PASSED\n");
}

void test_info_invalid(void) {
    printf("Test: get_info rejects invalid usage... ");
    reset();
    uint32_t id = mk(100u, 0u);
    elib_ts_task_info_t info;

    assert(elib_ts_task_get_info(&ctx, id, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_task_get_info(NULL, id, &info)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_task_get_info(&ctx, 99u, &info) == ELIB_TS_ERR_NOT_FOUND);

    (void)elib_ts_task_delete(&ctx, id);
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_ERR_NOT_FOUND);

    elib_ts_deinit(&ctx);
    assert(elib_ts_task_get_info(&ctx, id, &info)
           == ELIB_TS_ERR_NOT_INITIALIZED);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_get_info ===\n");
    test_info_initial();
    test_info_after_run();
    test_info_state_stopped();
    test_info_invalid();
    printf("=== test_get_info: all PASSED ===\n");
    return 0;
}
