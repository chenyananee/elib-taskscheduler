/* test_exec_time.c --- exec-time measurement (tick-quantized) / max tracking */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count;
static uint32_t ticks_during_exec;
static uint32_t timer_runs;

static void exec_cb(void *ud) {
    (void)ud;
    exec_count++;
}

/* Executes with a controllable number of simulated tick interrupts */
static void exec_cb_ticking(void *ud) {
    (void)ud;
    timer_runs++;
    for (uint32_t i = 0u; i < ticks_during_exec; i++) {
        elib_ts_tick_update(&ctx, 1u);
    }
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    ticks_during_exec = 0u;
    timer_runs = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, elib_ts_exec_cb_t ecb) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, ecb, NULL, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_exec_time_measured(void) {
    printf("Test: ticks during exec produce exec_time_ms... ");
    reset();
    ticks_during_exec = 3u;
    uint32_t id = mk(5u, exec_cb_ticking);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(timer_runs == 1u);
    assert(tasks[id].exec_time_ms == 3u);
    assert(tasks[id].max_exec_time_ms == 3u);
    printf("PASSED\n");
}

void test_exec_time_zero_without_ticks(void) {
    printf("Test: execution shorter than one tick reports 0... ");
    reset();
    ticks_during_exec = 0u;
    uint32_t id = mk(5u, exec_cb_ticking);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 0u);
    assert(tasks[id].max_exec_time_ms == 0u);
    printf("PASSED\n");
}

void test_max_exec_monotonic(void) {
    printf("Test: max_exec_time tracks the longest run only... ");
    reset();
    uint32_t id = mk(5u, exec_cb_ticking);
    (void)elib_ts_start(&ctx);

    /* run 1: 2 ticks */
    ticks_during_exec = 2u;
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 2u);
    assert(tasks[id].max_exec_time_ms == 2u);

    /* run 2: 1 tick (elapsed already progressed by 2 during run 1) */
    ticks_during_exec = 1u;
    elib_ts_tick_update(&ctx, 3u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 1u);
    assert(tasks[id].max_exec_time_ms == 2u);

    /* run 3: 4 ticks */
    ticks_during_exec = 4u;
    elib_ts_tick_update(&ctx, 4u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 4u);
    assert(tasks[id].max_exec_time_ms == 4u);
    printf("PASSED\n");
}

void test_elapsed_after_exec_is_wall_progress(void) {
    printf("Test: ticks during exec count toward the next period... ");
    reset();
    ticks_during_exec = 3u;
    uint32_t id = mk(100u, exec_cb_ticking);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 100u);
    (void)elib_ts_poll(&ctx);
    assert(tasks[id].exec_time_ms == 3u);
    assert(tasks[id].elapsed_ms == 3u);        /* wall-clock progress kept */
    printf("PASSED\n");
}

void test_info_exposes_exec_time(void) {
    printf("Test: task info exposes exec_time_ms... ");
    reset();
    ticks_during_exec = 2u;
    uint32_t id = mk(5u, exec_cb_ticking);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    elib_ts_task_info_t info;
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_OK);
    assert(info.exec_time_ms == 2u);
    assert(info.max_exec_time_ms == 2u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_exec_time ===\n");
    test_exec_time_measured();
    test_exec_time_zero_without_ticks();
    test_max_exec_monotonic();
    test_elapsed_after_exec_is_wall_progress();
    test_info_exposes_exec_time();
    (void)exec_cb;
    (void)exec_count;
    printf("=== test_exec_time: all PASSED ===\n");
    return 0;
}
