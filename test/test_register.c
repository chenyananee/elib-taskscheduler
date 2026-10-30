/* test_register.c --- register validation / pool / slot reuse / user_data */
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

static uint32_t mk(uint32_t period, uint32_t timeout,
                   elib_ts_exec_cb_t ecb, elib_ts_init_cb_t icb, void *ud) {
    uint32_t id;
    assert(elib_ts_register(&ctx, period, timeout, ecb, icb, NULL, ud, &id)
           == ELIB_TS_OK);
    return id;
}

void test_register_first(void) {
    printf("Test: register first task gets id 0 / READY... ");
    reset();
    int marker = 0;
    uint32_t id = mk(100u, 50u, exec_cb, NULL, &marker);
    assert(id == 0u);
    assert(tasks[0].state == ELIB_TS_TASK_READY);
    assert(tasks[0].period_ms == 100u);
    assert(tasks[0].timeout_ms == 50u);
    assert(tasks[0].elapsed_ms == 0u);
    assert(tasks[0].exec_time_ms == 0u);
    assert(tasks[0].max_exec_time_ms == 0u);
    assert(tasks[0].user_data == &marker);
    printf("PASSED\n");
}

void test_register_with_init(void) {
    printf("Test: register with init_cb enters INIT... ");
    reset();
    uint32_t id = mk(100u, 0u, exec_cb, init_cb, NULL);
    assert(tasks[id].state == ELIB_TS_TASK_INIT);
    printf("PASSED\n");
}

void test_register_sequential_ids(void) {
    printf("Test: register assigns sequential ids... ");
    reset();
    assert(mk(100u, 0u, exec_cb, NULL, NULL) == 0u);
    assert(mk(100u, 0u, exec_cb, NULL, NULL) == 1u);
    assert(mk(100u, 0u, exec_cb, NULL, NULL) == 2u);
    printf("PASSED\n");
}

void test_register_invalid(void) {
    printf("Test: register rejects null/zero params... ");
    reset();
    uint32_t id;
    assert(elib_ts_register(&ctx, 100u, 0u, NULL, NULL, NULL, NULL, &id)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_register(&ctx, 100u, 0u, exec_cb, NULL, NULL, NULL, NULL)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_register(NULL, 100u, 0u, exec_cb, NULL, NULL, NULL, &id)
           == ELIB_TS_ERR_INVALID_PARAM);
    assert(elib_ts_register(&ctx, 0u, 0u, exec_cb, NULL, NULL, NULL, &id)
           == ELIB_TS_ERR_INVALID_PARAM);
    printf("PASSED\n");
}

void test_register_pool_full(void) {
    printf("Test: register rejects when pool full... ");
    reset();
    for (uint32_t i = 0u; i < MAX_TASKS; i++) {
        (void)mk(100u, 0u, exec_cb, NULL, NULL);
    }
    uint32_t id;
    assert(elib_ts_register(&ctx, 100u, 0u, exec_cb, NULL, NULL, NULL, &id)
           == ELIB_TS_ERR_FULL);
    printf("PASSED\n");
}

void test_register_reuse_after_delete(void) {
    printf("Test: register reuses first freed slot with clean state... ");
    reset();
    for (uint32_t i = 0u; i < MAX_TASKS; i++) {
        (void)mk(100u, 0u, exec_cb, NULL, NULL);
    }
    assert(elib_ts_task_delete(&ctx, 2u) == ELIB_TS_OK);
    uint32_t id = mk(250u, 10u, exec_cb, NULL, NULL);
    assert(id == 2u);
    assert(tasks[2].period_ms == 250u);
    assert(tasks[2].elapsed_ms == 0u);
    assert(tasks[2].exec_time_ms == 0u);
    assert(tasks[2].state == ELIB_TS_TASK_READY);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_register ===\n");
    test_register_first();
    test_register_with_init();
    test_register_sequential_ids();
    test_register_invalid();
    test_register_pool_full();
    test_register_reuse_after_delete();
    printf("=== test_register: all PASSED ===\n");
    return 0;
}
