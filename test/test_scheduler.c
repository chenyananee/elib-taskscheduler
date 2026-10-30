/* test_scheduler.c --- scheduler start/stop / tick freeze / hook balance */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count;
static void exec_cb(void *ud) { (void)ud; exec_count++; }

static uint32_t hook_enter, hook_exit;
static void enter_hook(void) { hook_enter++; }
static void exit_hook(void) { hook_exit++; }

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    hook_enter = 0u;
    hook_exit = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, exec_cb, NULL, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_start_stop(void) {
    printf("Test: scheduler start/stop transitions... ");
    reset();
    assert(elib_ts_start(&ctx) == ELIB_TS_OK);
    assert(ctx.running == 1u);
    assert(elib_ts_start(&ctx) == ELIB_TS_ERR_ALREADY_RUNNING);
    assert(elib_ts_stop(&ctx) == ELIB_TS_OK);
    assert(ctx.running == 0u);
    assert(elib_ts_stop(&ctx) == ELIB_TS_ERR_NOT_RUNNING);
    printf("PASSED\n");
}

void test_tick_frozen_when_stopped(void) {
    printf("Test: ticks are frozen while the scheduler is stopped... ");
    reset();
    uint32_t id = mk(5u);

    /* not started yet */
    elib_ts_tick_update(&ctx, 10u);
    assert(tasks[id].elapsed_ms == 0u);

    /* started then stopped */
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 2u);
    assert(tasks[id].elapsed_ms == 2u);
    (void)elib_ts_stop(&ctx);
    elib_ts_tick_update(&ctx, 10u);
    assert(tasks[id].elapsed_ms == 2u);
    printf("PASSED\n");
}

void test_start_clears_elapsed(void) {
    printf("Test: start clears active task elapsed... ");
    reset();
    uint32_t id = mk(5u);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 3u);
    assert(tasks[id].elapsed_ms == 3u);

    (void)elib_ts_stop(&ctx);
    assert(elib_ts_start(&ctx) == ELIB_TS_OK);
    assert(tasks[id].elapsed_ms == 0u);
    printf("PASSED\n");
}

void test_poll_while_stopped(void) {
    printf("Test: poll is a no-op while stopped... ");
    reset();
    (void)mk(5u);
    elib_ts_tick_update(&ctx, 10u);
    assert(elib_ts_poll(&ctx) == ELIB_TS_OK);
    assert(exec_count == 0u);
    printf("PASSED\n");
}

void test_critical_hooks_balanced(void) {
    printf("Test: critical hooks are balanced around polling... ");
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    hook_enter = 0u;
    hook_exit = 0u;
    exec_count = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, enter_hook, exit_hook)
           == ELIB_TS_OK);

    uint32_t id;
    (void)elib_ts_register(&ctx, 5u, 0u, exec_cb, NULL, NULL, NULL, &id);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(exec_count == 1u);
    assert(hook_enter > 0u);
    assert(hook_enter == hook_exit);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_scheduler ===\n");
    test_start_stop();
    test_tick_frozen_when_stopped();
    test_start_clears_elapsed();
    test_poll_while_stopped();
    test_critical_hooks_balanced();
    printf("=== test_scheduler: all PASSED ===\n");
    return 0;
}
