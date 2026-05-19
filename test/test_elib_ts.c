/* test_elib_ts.c - Task Scheduler Unit Tests */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/elib_ts.h"

/* Test configuration */
#define MAX_TASKS 4
static elib_ts_ctx_t test_ctx;
static elib_ts_task_t test_tasks[MAX_TASKS];

/* Callback counters */
static int exec_count[4] = {0, 0, 0, 0};
static int init_count[4] = {0, 0, 0, 0};
static int anomaly_count = 0;
static elib_ts_anomaly_t last_anomaly_type;

/* Callback functions */
static void exec_cb0(void) { exec_count[0]++; }
static void exec_cb1(void) { exec_count[1]++; }
static void exec_cb2(void) { exec_count[2]++; }
static void exec_cb3(void) { exec_count[3]++; }
static void init_cb0(void) { init_count[0]++; }
static void init_cb1(void) { init_count[1]++; }
static void anomaly_cb(elib_ts_anomaly_t type) {
    anomaly_count++;
    last_anomaly_type = type;
}

/* Slow exec callback that simulates ISR ticks during execution */
static void slow_exec_cb(void) {
    exec_count[2]++;
    elib_ts_tick_update(&test_ctx, 1);
    elib_ts_tick_update(&test_ctx, 1);
    elib_ts_tick_update(&test_ctx, 1);
}

/* Reset test state */
static void reset_test(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(test_tasks, 0, sizeof(test_tasks));
    memset(exec_count, 0, sizeof(exec_count));
    memset(init_count, 0, sizeof(init_count));
    anomaly_count = 0;
    elib_ts_init(&test_ctx, test_tasks, MAX_TASKS, NULL, NULL);
}

/* === Init/Deinit Tests === */

static void test_init_valid(void) {
    printf("Test: init valid... ");
    reset_test();

    assert(test_ctx.initialized == 1);
    assert(test_ctx.max_tasks == MAX_TASKS);
    assert(test_ctx.running == false);

    printf("PASSED\n");
}

static void test_init_null_ctx(void) {
    printf("Test: init null ctx... ");
    reset_test();

    elib_ts_err_t err = elib_ts_init(NULL, test_tasks, MAX_TASKS, NULL, NULL);
    assert(err == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_init_null_tasks(void) {
    printf("Test: init null tasks... ");
    reset_test();

    elib_ts_err_t err = elib_ts_init(&test_ctx, NULL, MAX_TASKS, NULL, NULL);
    assert(err == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_init_zero_max(void) {
    printf("Test: init zero max... ");
    reset_test();

    elib_ts_err_t err = elib_ts_init(&test_ctx, test_tasks, 0, NULL, NULL);
    assert(err == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_deinit(void) {
    printf("Test: deinit... ");
    reset_test();

    elib_ts_deinit(&test_ctx);
    assert(test_ctx.initialized == 0);

    printf("PASSED\n");
}

static void test_uninitialized(void) {
    printf("Test: uninitialized rejects ops... ");
    reset_test();

    elib_ts_deinit(&test_ctx);

    uint32_t task_id;
    assert(elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id)
           == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_start(&test_ctx) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_stop(&test_ctx) == ELIB_TS_ERR_NOT_INITIALIZED);
    assert(elib_ts_poll(&test_ctx) == ELIB_TS_ERR_NOT_INITIALIZED);

    printf("PASSED\n");
}

/* === Register Tests === */

static void test_register_valid(void) {
    printf("Test: register valid... ");
    reset_test();

    uint32_t task_id;
    elib_ts_err_t err = elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
    assert(err == ELIB_TS_OK);
    assert(task_id == 0);
    assert(test_tasks[0].state == ELIB_TS_TASK_READY);
    assert(test_tasks[0].period_ms == 100);

    printf("PASSED\n");
}

static void test_register_with_init(void) {
    printf("Test: register with init... ");
    reset_test();

    uint32_t task_id;
    elib_ts_err_t err = elib_ts_register(&test_ctx, 100, 0, exec_cb0, init_cb0, NULL, &task_id);
    assert(err == ELIB_TS_OK);
    assert(test_tasks[task_id].state == ELIB_TS_TASK_INIT);

    printf("PASSED\n");
}

static void test_register_null_exec(void) {
    printf("Test: register null exec... ");
    reset_test();

    uint32_t task_id;
    elib_ts_err_t err = elib_ts_register(&test_ctx, 100, 0, NULL, NULL, NULL, &task_id);
    assert(err == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_register_zero_period(void) {
    printf("Test: register zero period... ");
    reset_test();

    uint32_t task_id;
    elib_ts_err_t err = elib_ts_register(&test_ctx, 0, 0, exec_cb0, NULL, NULL, &task_id);
    assert(err == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

static void test_register_full(void) {
    printf("Test: register pool full... ");
    reset_test();

    uint32_t task_id;
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        elib_ts_err_t err = elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
        assert(err == ELIB_TS_OK);
    }

    elib_ts_err_t err = elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
    assert(err == ELIB_TS_ERR_FULL);

    printf("PASSED\n");
}

/* === Scheduler Start/Stop Tests === */

static void test_scheduler_start_stop(void) {
    printf("Test: scheduler start/stop... ");
    reset_test();

    assert(elib_ts_start(&test_ctx) == ELIB_TS_OK);
    assert(test_ctx.running == true);
    assert(elib_ts_start(&test_ctx) == ELIB_TS_ERR_ALREADY_RUNNING);

    assert(elib_ts_stop(&test_ctx) == ELIB_TS_OK);
    assert(test_ctx.running == false);
    assert(elib_ts_stop(&test_ctx) == ELIB_TS_ERR_NOT_RUNNING);

    printf("PASSED\n");
}

/* === Task Start/Stop Tests === */

static void test_task_start_stop(void) {
    printf("Test: task start/stop... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);

    assert(elib_ts_task_stop(&test_ctx, task_id) == ELIB_TS_OK);
    assert(test_tasks[task_id].state == ELIB_TS_TASK_STOPPED);

    assert(elib_ts_task_start(&test_ctx, task_id) == ELIB_TS_OK);
    assert(test_tasks[task_id].state == ELIB_TS_TASK_READY);

    printf("PASSED\n");
}

static void test_task_stop_stopped(void) {
    printf("Test: task stop stopped... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
    elib_ts_task_stop(&test_ctx, task_id);

    assert(elib_ts_task_stop(&test_ctx, task_id) == ELIB_TS_ERR_TASK_STOPPED);

    printf("PASSED\n");
}

static void test_task_start_running(void) {
    printf("Test: task start running... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);

    assert(elib_ts_task_start(&test_ctx, task_id) == ELIB_TS_ERR_TASK_RUNNING);

    printf("PASSED\n");
}

static void test_task_start_with_init(void) {
    printf("Test: task start with init... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, init_cb0, NULL, &task_id);

    elib_ts_task_stop(&test_ctx, task_id);
    assert(elib_ts_task_start(&test_ctx, task_id) == ELIB_TS_OK);
    assert(test_tasks[task_id].state == ELIB_TS_TASK_INIT);

    printf("PASSED\n");
}

/* === Set Period Tests === */

static void test_set_period(void) {
    printf("Test: set period... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);

    assert(elib_ts_task_set_period(&test_ctx, task_id, 200) == ELIB_TS_OK);
    assert(test_tasks[task_id].period_ms == 200);

    printf("PASSED\n");
}

static void test_set_period_zero(void) {
    printf("Test: set period zero... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);

    assert(elib_ts_task_set_period(&test_ctx, task_id, 0) == ELIB_TS_ERR_INVALID_PARAM);

    printf("PASSED\n");
}

/* === Get Info Test === */

static void test_get_info(void) {
    printf("Test: get info... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 50, exec_cb0, NULL, NULL, &task_id);

    elib_ts_task_info_t info;
    assert(elib_ts_task_get_info(&test_ctx, task_id, &info) == ELIB_TS_OK);
    assert(info.period_ms == 100);
    assert(info.timeout_ms == 50);
    assert(info.state == ELIB_TS_TASK_READY);

    printf("PASSED\n");
}

/* === Tick Update Tests === */

static void test_tick_update(void) {
    printf("Test: tick update... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);

    assert(test_tasks[task_id].elapsed_ms == 0);
    elib_ts_tick_update(&test_ctx, 1);
    assert(test_tasks[task_id].elapsed_ms == 1);
    elib_ts_tick_update(&test_ctx, 2);
    assert(test_tasks[task_id].elapsed_ms == 3);

    printf("PASSED\n");
}

static void test_tick_skip_stopped(void) {
    printf("Test: tick skip stopped... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
    elib_ts_task_stop(&test_ctx, task_id);

    elib_ts_tick_update(&test_ctx, 1);
    assert(test_tasks[task_id].elapsed_ms == 0);

    printf("PASSED\n");
}

static void test_tick_overflow(void) {
    printf("Test: tick overflow... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 100, 0, exec_cb0, NULL, NULL, &task_id);
    test_tasks[task_id].elapsed_ms = UINT32_MAX;

    elib_ts_tick_update(&test_ctx, 1);
    assert(test_tasks[task_id].elapsed_ms == UINT32_MAX);

    printf("PASSED\n");
}

/* === Poll Scheduling Tests === */

static void test_poll_dispatch(void) {
    printf("Test: poll dispatch... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 0, exec_cb0, NULL, NULL, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 4; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(exec_count[0] == 0);

    elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(exec_count[0] == 1);

    printf("PASSED\n");
}

static void test_poll_init_then_exec(void) {
    printf("Test: poll init then exec... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 0, exec_cb0, init_cb0, NULL, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);

    assert(init_count[0] == 1);
    assert(exec_count[0] == 0);

    elib_ts_tick_update(&test_ctx, 1);
    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(exec_count[0] == 1);

    printf("PASSED\n");
}

static void test_poll_priority(void) {
    printf("Test: poll priority... ");
    reset_test();

    uint32_t id0, id1;
    elib_ts_register(&test_ctx, 5, 0, exec_cb0, NULL, NULL, &id0);
    elib_ts_register(&test_ctx, 5, 0, exec_cb1, NULL, NULL, &id1);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);

    assert(exec_count[0] == 1);
    assert(exec_count[1] == 1);

    printf("PASSED\n");
}

static void test_poll_scheduler_stopped(void) {
    printf("Test: poll scheduler stopped... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 0, exec_cb0, NULL, NULL, &task_id);

    for (int i = 0; i < 10; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(exec_count[0] == 0);

    printf("PASSED\n");
}

/* === Timing Measurement + Anomaly Tests === */

static void test_exec_time(void) {
    printf("Test: exec time measurement... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 0, slow_exec_cb, NULL, NULL, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);

    assert(test_tasks[task_id].exec_time_ms == 3);

    printf("PASSED\n");
}

static void test_max_exec_time(void) {
    printf("Test: max exec time tracking... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 2, 0, slow_exec_cb, NULL, NULL, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 2; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(test_tasks[task_id].max_exec_time_ms == 3);

    for (int i = 0; i < 2; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);
    assert(test_tasks[task_id].max_exec_time_ms == 3);

    printf("PASSED\n");
}

static void test_timeout_anomaly(void) {
    printf("Test: timeout anomaly... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 2, slow_exec_cb, NULL, anomaly_cb, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);

    assert(anomaly_count == 1);
    assert(last_anomaly_type == ELIB_TS_ANOMALY_TIMEOUT);

    printf("PASSED\n");
}

static void test_no_anomaly_zero_timeout(void) {
    printf("Test: no anomaly zero timeout... ");
    reset_test();

    uint32_t task_id;
    elib_ts_register(&test_ctx, 5, 0, slow_exec_cb, NULL, anomaly_cb, &task_id);
    elib_ts_start(&test_ctx);

    for (int i = 0; i < 5; i++) elib_ts_tick_update(&test_ctx, 1);
    elib_ts_poll(&test_ctx);

    assert(anomaly_count == 0);

    printf("PASSED\n");
}

/* === Main === */

int main(void) {
    printf("=== elib-taskscheduler tests ===\n\n");

    /* Init/Deinit tests */
    test_init_valid();
    test_init_null_ctx();
    test_init_null_tasks();
    test_init_zero_max();
    test_deinit();
    test_uninitialized();

    /* Register tests */
    test_register_valid();
    test_register_with_init();
    test_register_null_exec();
    test_register_zero_period();
    test_register_full();

    /* Scheduler start/stop tests */
    test_scheduler_start_stop();

    /* Task start/stop tests */
    test_task_start_stop();
    test_task_stop_stopped();
    test_task_start_running();
    test_task_start_with_init();

    /* Set period tests */
    test_set_period();
    test_set_period_zero();

    /* Get info test */
    test_get_info();

    /* Tick update tests */
    test_tick_update();
    test_tick_skip_stopped();
    test_tick_overflow();

    /* Poll scheduling tests */
    test_poll_dispatch();
    test_poll_init_then_exec();
    test_poll_priority();
    test_poll_scheduler_stopped();

    /* Timing and anomaly tests */
    test_exec_time();
    test_max_exec_time();
    test_timeout_anomaly();
    test_no_anomaly_zero_timeout();

    printf("\n=== All tests passed ===\n");
    return 0;
}
