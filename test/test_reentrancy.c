/* test_reentrancy.c --- callback self-operations must not be overwritten */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "elib_ts.h"

#define MAX_TASKS 4
static elib_ts_ctx_t ctx;
static elib_ts_task_t tasks[MAX_TASKS];

static uint32_t exec_count;
static uint32_t init_count;
static uint32_t self_id;
static void *exec_ud;
static void *init_ud;

static void exec_cb(void *ud) {
    (void)ud;
    exec_count++;
}

static void exec_cb_self_stop(void *ud) {
    (void)ud;
    exec_count++;
    assert(elib_ts_task_stop(&ctx, self_id) == ELIB_TS_OK);
}

static void exec_cb_self_delete(void *ud) {
    (void)ud;
    exec_count++;
    assert(elib_ts_task_delete(&ctx, self_id) == ELIB_TS_OK);
}

static void exec_cb_stop_then_start(void *ud) {
    (void)ud;
    exec_count++;
    assert(elib_ts_task_stop(&ctx, self_id) == ELIB_TS_OK);
    assert(elib_ts_task_start(&ctx, self_id) == ELIB_TS_OK);
}

static void exec_cb_capture_ud(void *ud) {
    exec_ud = ud;
    exec_count++;
}

static void init_cb_self_stop(void *ud) {
    (void)ud;
    init_count++;
    assert(elib_ts_task_stop(&ctx, self_id) == ELIB_TS_OK);
}

static void init_cb_capture_ud(void *ud) {
    init_ud = ud;
    init_count++;
}

static void reset(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(tasks, 0, sizeof(tasks));
    exec_count = 0u;
    init_count = 0u;
    self_id = 0u;
    exec_ud = NULL;
    init_ud = NULL;
    assert(elib_ts_init(&ctx, tasks, MAX_TASKS, NULL, NULL) == ELIB_TS_OK);
}

void test_exec_self_stop(void) {
    printf("Test: exec callback stopping itself stays STOPPED... ");
    reset();
    self_id = 0u;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb_self_stop, NULL, NULL,
                            NULL, &id) == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(exec_count == 1u);
    assert(tasks[id].state == ELIB_TS_TASK_STOPPED);  /* not resurrected */

    elib_ts_tick_update(&ctx, 50u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 1u);                         /* stays stopped */

    assert(elib_ts_task_start(&ctx, id) == ELIB_TS_OK);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 2u);
    assert(tasks[id].state == ELIB_TS_TASK_STOPPED);
    printf("PASSED\n");
}

void test_exec_self_delete(void) {
    printf("Test: exec callback deleting itself frees the slot... ");
    reset();
    self_id = 0u;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb_self_delete, NULL, NULL,
                            NULL, &id) == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(exec_count == 1u);
    assert(tasks[id].state == ELIB_TS_TASK_UNUSED);
    assert(tasks[id].exec_time_ms == 0u);   /* no bookkeeping after delete */

    /* slot is reusable and further ticking is safe */
    elib_ts_tick_update(&ctx, 50u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 1u);

    uint32_t id2;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb, NULL, NULL, NULL, &id2)
           == ELIB_TS_OK);
    assert(id2 == id);
    printf("PASSED\n");
}

void test_init_self_stop(void) {
    printf("Test: init callback stopping itself stays STOPPED... ");
    reset();
    self_id = 0u;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb, init_cb_self_stop, NULL,
                            NULL, &id) == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(init_count == 1u);
    assert(tasks[id].state == ELIB_TS_TASK_STOPPED);  /* not forced to READY */
    elib_ts_tick_update(&ctx, 50u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 0u);
    printf("PASSED\n");
}

void test_exec_stop_then_start(void) {
    printf("Test: exec callback restarting itself ends in READY... ");
    reset();
    self_id = 0u;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb_stop_then_start, NULL,
                            NULL, NULL, &id) == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);

    assert(exec_count == 1u);
    assert(tasks[id].state == ELIB_TS_TASK_READY);    /* left as the callback set it */

    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);
    assert(exec_count == 2u);
    printf("PASSED\n");
}

void test_user_data_delivery(void) {
    printf("Test: user_data reaches init and exec callbacks... ");
    reset();
    int marker = 0;
    uint32_t id;
    assert(elib_ts_register(&ctx, 5u, 0u, exec_cb_capture_ud,
                            init_cb_capture_ud, NULL, &marker, &id)
           == ELIB_TS_OK);
    (void)elib_ts_start(&ctx);
    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);                 /* init runs */
    assert(init_count == 1u);
    assert(init_ud == &marker);

    elib_ts_tick_update(&ctx, 5u);
    (void)elib_ts_poll(&ctx);                 /* exec runs */
    assert(exec_count == 1u);
    assert(exec_ud == &marker);
    printf("PASSED\n");
}

int main(void) {
    printf("=== test_reentrancy ===\n");
    test_exec_self_stop();
    test_exec_self_delete();
    test_init_self_stop();
    test_exec_stop_then_start();
    test_user_data_delivery();
    printf("=== test_reentrancy: all PASSED ===\n");
    return 0;
}
