/* test_set_period.c --- runtime period change / state coverage */
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

static uint32_t self_id;
static uint32_t self_new_period;
static void exec_self_period(void *ud) {
    (void)ud;
    exec_count++;
    assert(elib_ts_task_set_period(&ctx, self_id, self_new_period)
           == ELIB_TS_OK);
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    self_id = 0u;
    self_new_period = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(uint32_t period, elib_ts_exec_cb_t ecb, elib_ts_init_cb_t icb) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, 0u, ecb, icb, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_set_period_ready(void) {
    printf("Test: set_period on READY task... ");
    reset();
    uint32_t id = mk(100u, exec_cb, NULL);
    assert(elib_ts_task_set_period(&ctx, id, 200u) == ELIB_TS_OK);
    assert(tasks[id].period_ms == 200u);
    printf("PASSED\n");
}

void test_set_period_keeps_elapsed(void) {
    printf("Test: set_period keeps elapsed as absolute progress... ");
    reset();
    uint32_t id = mk(100u, exec_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 50u);
    assert(tasks[id].elapsed_ms == 50u);

    assert(elib_ts_task_set_period(&ctx, id, 200u) == ELIB_TS_OK);
    assert(tasks[id].elapsed_ms == 50u);       /* unscaled */

    elib_ts_tick_update(&ctx, 150u);
    assert(elib_ts_poll(&ctx) == ELIB_TS_OK);
    assert(exec_count == 1u);                  /* due at 200 total */
    printf("PASSED\n");
}

void test_set_period_in_init(void) {
    printf("Test: set_period allowed on INIT task... ");
    reset();
    uint32_t id = mk(100u, exec_cb, init_cb);
    assert(tasks[id].state == ELIB_TS_TASK_INIT);
    assert(elib_ts_task_set_period(&ctx, id, 300u) == ELIB_TS_OK);
    assert(tasks[id].period_ms == 300u);
    printf("PASSED\n");
}

void test_set_period_inside_exec(void) {
    printf("Test: set_period allowed from the task's own exec callback... ");
    reset();
    self_id = 0u;
    self_new_period = 50u;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_self_period, NULL, NULL,
                            NULL, &id) == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 1u);
    assert(tasks[id].period_ms == 50u);

    /* next cycle honours the new period (elapsed restarts from ticks in exec) */
    elib_ts_tick_update(&ctx, 50u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 2u);
    printf("PASSED\n");
}

void test_set_period_invalid(void) {
    printf("Test: set_period rejects zero / unused / bad id... ");
    reset();
    uint32_t id = mk(100u, exec_cb, NULL);
    assert(elib_ts_task_set_period(&ctx, id, 0u) == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_task_set_period(&ctx, 99u, 10u) == ELIB_TS_ERR_NOT_FOUND);
    (void)elib_ts_task_delete(&ctx, id);
    assert(elib_ts_task_set_period(&ctx, id, 10u) == ELIB_TS_ERR_NOT_FOUND);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_set_period ===\n");
    test_set_period_ready();
    test_set_period_keeps_elapsed();
    test_set_period_in_init();
    test_set_period_inside_exec();
    test_set_period_invalid();
    printf("=== test_set_period: all PASSED ===\n");
    return 0;
}
