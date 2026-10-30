/* test_poll_dispatch.c --- dispatch edge / once-per-pass / order / INIT path */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count[4];
static uint32_t init_count[4];
static uint32_t order_log[8];
static uint32_t order_len;

static void exec_cb(void *ud) {
    uint32_t slot = (uint32_t)(uintptr_t)ud;
    exec_count[slot]++;
}

static void init_cb(void *ud) {
    uint32_t slot = (uint32_t)(uintptr_t)ud;
    init_count[slot]++;
}

static void order_cb(void *ud) {
    order_log[order_len++] = (uint32_t)(uintptr_t)ud;
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    memset(exec_count, 0, sizeof(exec_count));
    memset(init_count, 0, sizeof(init_count));
    memset(order_log, 0, sizeof(order_log));
    order_len = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, elib_ts_exec_cb_t ecb, void *ud) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, ecb, NULL, NULL, ud, &id)
           == ELIB_TS_OK);
    return id;
}

void test_not_due(void) {
    printf("Test: task not due yet does not run... ");
    reset();
    (void)mk(5u, exec_cb, (void *)(uintptr_t)0u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 4u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 0u);
    printf("PASSED\n");
}

void test_due_exact(void) {
    printf("Test: elapsed == period dispatches exactly once... ");
    reset();
    (void)mk(5u, exec_cb, (void *)(uintptr_t)0u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 1u);
    assert(tasks[0].elapsed_ms == 0u);
    assert(tasks[0].state == ELIB_TS_TASK_READY);
    printf("PASSED\n");
}

void test_once_per_pass_no_catch_up(void) {
    printf("Test: large overshoot runs once per pass (no catch-up)... ");
    reset();
    (void)mk(5u, exec_cb, (void *)(uintptr_t)0u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 100u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 1u);               /* not 20 */
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 1u);               /* surplus time discarded */
    printf("PASSED\n");
}

void test_priority_order(void) {
    printf("Test: dispatch order follows array index... ");
    reset();
    (void)mk(5u, order_cb, (void *)(uintptr_t)0u);
    (void)mk(5u, order_cb, (void *)(uintptr_t)1u);
    (void)mk(5u, order_cb, (void *)(uintptr_t)2u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(order_len == 3u);
    assert(order_log[0] == 0u && order_log[1] == 1u && order_log[2] == 2u);
    printf("PASSED\n");
}

void test_stopped_task_skipped(void) {
    printf("Test: stopped task is not dispatched... ");
    reset();
    (void)mk(5u, exec_cb, (void *)(uintptr_t)0u);
    (void)mk(5u, exec_cb, (void *)(uintptr_t)1u);
    (void)elib_ts_start(&ctx);
    (void)elib_ts_task_stop(&ctx, 1u);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 1u);
    assert(exec_count[1] == 0u);
    printf("PASSED\n");
}

void test_init_then_exec(void) {
    printf("Test: INIT runs init once, exec starts on the next cycle... ");
    reset();
    uint32_t id;
    (void)elib_ts_register(&ctx, 5u, 0u, exec_cb, init_cb, NULL,
                           (void *)(uintptr_t)0u, &id);
    (void)elib_ts_start(&ctx);

    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(init_count[0] == 1u);
    assert(exec_count[0] == 0u);               /* exec waits for a full period */
    assert(tasks[id].elapsed_ms == 0u);

    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count[0] == 1u);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_poll_dispatch ===\n");
    test_not_due();
    test_due_exact();
    test_once_per_pass_no_catch_up();
    test_priority_order();
    test_stopped_task_skipped();
    test_init_then_exec();
    printf("=== test_poll_dispatch: all PASSED ===\n");
    return 0;
}
