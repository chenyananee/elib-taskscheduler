/* test_tick.c --- tick accumulation / freeze / saturation / no-op paths */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count;
static void exec_cb(void *ud) { (void)ud; exec_count++; }
static void init_cb(void *ud) { (void)ud; }

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, elib_ts_init_cb_t icb) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, exec_cb, icb, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_tick_accumulate(void) {
    printf("Test: tick accumulates elapsed... ");
    reset();
    uint32_t id = mk(100u, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 1u);
    elib_ts_tick_update(&ctx, 2u);
    assert(tasks[id].elapsed_ms == 3u);
    printf("PASSED\n");
}

void test_tick_skips_stopped_and_unused(void) {
    printf("Test: tick skips stopped and unused slots... ");
    reset();
    uint32_t id0 = mk(100u, NULL);
    uint32_t id1 = mk(100u, NULL);
    (void)elib_ts_start(&ctx);
    (void)elib_ts_task_stop(&ctx, id1);
    elib_ts_tick_update(&ctx, 5u);
    assert(tasks[id0].elapsed_ms == 5u);
    assert(tasks[id1].elapsed_ms == 0u);

    (void)elib_ts_task_delete(&ctx, id0);
    elib_ts_tick_update(&ctx, 5u);
    assert(tasks[id0].elapsed_ms == 0u);       /* unused slot untouched */
    printf("PASSED\n");
}

void test_tick_counts_init_state(void) {
    printf("Test: tick accumulates while task is in INIT... ");
    reset();
    uint32_t id = mk(100u, init_cb);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    assert(tasks[id].elapsed_ms == 5u);

    (void)elib_ts_poll(&ctx);                  /* init runs, elapsed resets */
    assert(tasks[id].state == ELIB_TS_TASK_READY);
    assert(tasks[id].elapsed_ms == 0u);
    printf("PASSED\n");
}

void test_tick_saturation(void) {
    printf("Test: tick saturates at UINT32_MAX... ");
    reset();
    uint32_t id = mk(100u, NULL);
    (void)elib_ts_start(&ctx);
    tasks[id].elapsed_ms = UINT32_MAX;
    elib_ts_tick_update(&ctx, 1u);
    assert(tasks[id].elapsed_ms == UINT32_MAX);
    printf("PASSED\n");
}

void test_tick_zero_and_null(void) {
    printf("Test: tick(0) and tick(NULL) are no-ops... ");
    reset();
    uint32_t id = mk(100u, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 0u);
    assert(tasks[id].elapsed_ms == 0u);
    elib_ts_tick_update(NULL, 5u);             /* must not crash */
    assert(tasks[id].elapsed_ms == 0u);
    printf("PASSED\n");
}

void test_tick_after_deinit(void) {
    printf("Test: tick after deinit is a no-op... ");
    reset();
    uint32_t id = mk(100u, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_deinit(&ctx);
    elib_ts_tick_update(&ctx, 7u);
    assert(tasks[id].elapsed_ms == 0u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_tick ===\n");
    test_tick_accumulate();
    test_tick_skips_stopped_and_unused();
    test_tick_counts_init_state();
    test_tick_saturation();
    test_tick_zero_and_null();
    test_tick_after_deinit();
    printf("=== test_tick: all PASSED ===\n");
    return 0;
}
