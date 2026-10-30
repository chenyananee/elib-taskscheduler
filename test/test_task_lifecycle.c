/* test_task_lifecycle.c --- task start/stop/delete / state gates */
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
static void anomaly_cb(elib_ts_anomaly_t t, void *ud) { (void)t; (void)ud; }

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

static uint32_t mk(elib_ts_exec_cb_t ecb, elib_ts_init_cb_t icb) {
    uint32_t id;
    assert(elib_ts_register(&ctx, 100u, 0u, ecb, icb, NULL, NULL, &id)
           == ELIB_TS_OK);
    return id;
}

void test_stop_start_ready(void) {
    printf("Test: stop READY then start returns to READY... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    assert(elib_ts_task_stop(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].state == ELIB_TS_TASK_STOPPED);
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].state == ELIB_TS_TASK_READY);
    printf("PASSED\n");
}

void test_stop_stopped(void) {
    printf("Test: stopping a stopped task is rejected... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    (void)elib_ts_task_stop(&ctx, id);
    assert(elib_ts_task_stop(&ctx, id) == ELIB_TS_ERR_TASK_STOPPED);
    printf("PASSED\n");
}

void test_start_running(void) {
    printf("Test: starting a non-stopped task is rejected... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_ERR_TASK_RUNNING);
    printf("PASSED\n");
}

void test_start_with_init(void) {
    printf("Test: start with init_cb enters INIT... ");
    reset();
    uint32_t id = mk(exec_cb, init_cb);
    (void)elib_ts_task_stop(&ctx, id);
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].state == ELIB_TS_TASK_INIT);
    printf("PASSED\n");
}

void test_start_resets_elapsed(void) {
    printf("Test: task start resets elapsed... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 30u);
    assert(tasks[id].elapsed_ms == 30u);

    (void)elib_ts_task_stop(&ctx, id);
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].elapsed_ms == 0u);
    printf("PASSED\n");
}

void test_delete(void) {
    printf("Test: delete frees the slot, double delete rejected... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    assert(elib_ts_task_delete(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].state == ELIB_TS_TASK_UNUSED);
    assert(elib_ts_task_delete(&ctx, id) == ELIB_TS_ERR_NOT_FOUND);
    printf("PASSED\n");
}

void test_delete_bad_id(void) {
    printf("Test: delete rejects out-of-range ids... ");
    reset();
    assert(elib_ts_task_delete(&ctx, MAX_TASKS) == ELIB_TS_ERR_NOT_FOUND);
    assert(elib_ts_task_delete(&ctx, 99u) == ELIB_TS_ERR_NOT_FOUND);
    assert(elib_ts_task_delete(&ctx, 0xFFFFFFFFu) == ELIB_TS_ERR_NOT_FOUND);
    printf("PASSED\n");
}

void test_ops_on_unused(void) {
    printf("Test: APIs reject unused slots... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    (void)elib_ts_task_delete(&ctx, id);

    elib_ts_task_info_t info;
    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_ERR_NOT_FOUND);
    assert(elib_ts_task_stop(&ctx, id) == ELIB_TS_ERR_NOT_FOUND);
    assert(elib_ts_task_set_period(&ctx, id, 10u) == ELIB_TS_ERR_NOT_FOUND);
    assert(elib_ts_task_get_info(&ctx, id, &info) == ELIB_TS_ERR_NOT_FOUND);
    printf("PASSED\n");
}

void test_delete_stopped(void) {
    printf("Test: deleting a stopped task is allowed... ");
    reset();
    uint32_t id = mk(exec_cb, NULL);
    (void)elib_ts_task_stop(&ctx, id);
    assert(elib_ts_task_delete(&ctx, id) == ELIB_TS_OK);
    assert(tasks[id].state == ELIB_TS_TASK_UNUSED);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_task_lifecycle ===\n");
    test_stop_start_ready();
    test_stop_stopped();
    test_start_running();
    test_start_with_init();
    test_start_resets_elapsed();
    test_delete();
    test_delete_bad_id();
    test_ops_on_unused();
    test_delete_stopped();
    (void)anomaly_cb;
    printf("=== test_task_lifecycle: all PASSED ===\n");
    return 0;
}
