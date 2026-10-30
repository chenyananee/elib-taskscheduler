/* test_init.c --- init / deinit / hook-pair validation / re-init */
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

static uint32_t mk(uint32_t period, elib_ts_exec_cb_t cb, elib_ts_init_cb_t icb) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, cb, icb, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_init_valid(void) {
    printf("Test: init valid... ");
    reset();
    assert(ctx.initialized == 1u);
    assert(ctx.running == 0u);
    assert(ctx.max_tasks == MAX_TASKS);
    assert(ctx.enter_critical == NULL && ctx.exit_critical == NULL);
    printf("PASSED\n");
}

void test_init_invalid_params(void) {
    printf("Test: init rejects null/zero params... ");
    assert(elib_ts_init(NULL, tasks, MAX_TASKS, NULL, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_init(&ctx, NULL, MAX_TASKS, NULL, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_init(&ctx, tasks, 0u, NULL, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_init_hook_pair(void) {
    printf("Test: init rejects one-sided critical-hook pair... ");
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, enter_hook, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, exit_hook)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, enter_hook, exit_hook)
           == ELIB_TS_OK);
    assert(ctx.enter_critical == enter_hook);
    assert(ctx.exit_critical == exit_hook);
    printf("PASSED\n");
}

void test_deinit(void) {
    printf("Test: deinit clears flags... ");
    reset();
    assert(elib_ts_start(&ctx) == ELIB_TS_OK);
    elib_ts_deinit(&ctx);
    assert(ctx.initialized == 0u);
    assert(ctx.running == 0u);
    printf("PASSED\n");
}

void test_ops_reject_after_deinit(void) {
    printf("Test: all APIs reject after deinit... ");
    reset();
    uint32_t id = mk(100u, exec_cb, NULL);
    elib_ts_deinit(&ctx);

    uint32_t out;
    elib_ts_task_info_t info;
    assert(elib_ts_register(&ctx, 100u, 0u, exec_cb, NULL, NULL, NULL, &out)
           == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_start(&ctx) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_stop(&ctx) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_poll(&ctx) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_task_stop(&ctx, id) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_task_delete(&ctx, id) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_task_set_period(&ctx, id, 50u) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_ERR_NOT_INITIALIZED);
    printf("PASSED\n");
}

void test_reinit_over_dirty(void) {
    printf("Test: re-init clears previous tasks... ");
    reset();
    (void)mk(100u, exec_cb, NULL);
    (void)mk(100u, exec_cb, NULL);
    assert(tasks[0].state == ELIB_TS_TASK_READY);

    elib_ts_deinit(&ctx);
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
    assert(ctx.initialized == 1u);
    assert(tasks[0].state == ELIB_TS_TASK_UNUSED);
    assert(tasks[1].state == ELIB_TS_TASK_UNUSED);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_init ===\n");
    test_init_valid();
    test_init_invalid_params();
    test_init_hook_pair();
    test_deinit();
    test_ops_reject_after_deinit();
    test_reinit_over_dirty();
    printf("=== test_init: all PASSED ===\n");
    return 0;
}
