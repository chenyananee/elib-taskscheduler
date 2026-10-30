[简体中文](README.md) | **English**

# elib-taskscheduler

A lightweight task scheduler for resource-constrained embedded systems: zero dynamic memory, no global variables, pure C99, single umbrella-header API.

Current version: **2.0.0** (`ELIB_TS_VERSION`), single header `include/elib_ts.h`.

## Features

- **Zero dynamic memory / no globals**: all state lives in the user-supplied static `ctx + task array`
- **Single umbrella-header API**: all public types and functions live in `elib_ts.h`; no sub-headers
- **ISR / main-loop split**: the tick interrupt only accumulates milliseconds, `elib_ts_poll()` dispatches in the main loop, and callbacks never run inside an interrupt
- **The array is the priority**: the index is the dispatch order (index 0 first), strictly non-preemptive
- **Optional init callback**: `init_cb` runs once first, and only then does periodic execution begin
- **Execution time measurement**: `exec_time_ms` / `max_exec_time_ms` (tick-quantized)
- **Timeout anomaly detection**: the `timeout_ms` threshold raises `ELIB_TS_ANOMALY_TIMEOUT`
- **user_data pass-through**: the init / exec / anomaly callbacks all receive the user pointer
- **Optional critical-section hooks**: `init` rejects one-sided hooks (they must be paired)
- **Pure C99 implementation**, C++ compatible (`extern "C"`)

## Quick Start

### 1. Statically Define the Context and Task Pool

```c
#include "elib_ts.h"   /* add -I<repo>/include at build time */

#define MAX_TASKS 4

static elib_ts_ctx_t  g_sched_ctx;
static elib_ts_task_t g_tasks[MAX_TASKS];

static void app_init(void)
{
    /* no critical hooks: declares single-context use */
    elib_ts_init(&g_sched_ctx, g_tasks, MAX_TASKS, NULL, NULL);
}
```

### 2. Register a Task and Start the Scheduler

```c
typedef struct {
    uint32_t blink_count;
} led_state_t;

static led_state_t g_led;

static void led_init(void *user_data)
{
    led_state_t *st = (led_state_t *)user_data;
    st->blink_count = 0u;
    led_off();
}

static void led_toggle(void *user_data)
{
    led_state_t *st = (led_state_t *)user_data;
    st->blink_count++;
    toggle_led();
}

static uint32_t g_led_task_id;

static void app_start(void)
{
    elib_ts_register(&g_sched_ctx,
                     500u,            /* period_ms  */
                     50u,             /* timeout_ms, 0 disables it */
                     led_toggle,      /* exec_cb, required */
                     led_init,        /* init_cb, may be NULL */
                     NULL,            /* anomaly_cb, may be NULL */
                     &g_led,          /* user_data  */
                     &g_led_task_id);

    elib_ts_start(&g_sched_ctx);      /* clears elapsed of every active task */
}
```

### 3. The ISR Feeds the Tick, the Main Loop Dispatches

```c
/* 1ms tick interrupt: accumulate only, run no user code */
void SysTick_Handler(void)
{
    elib_ts_tick_update(&g_sched_ctx, 1u);
}

/* main loop */
for (;;) {
    elib_ts_poll(&g_sched_ctx);
    /* ... other background logic ... */
}
```

## API Overview

| Group | Function | Description |
|------|------|------|
| Lifetime | `elib_ts_init(ctx, tasks, max_tasks, enter_critical, exit_critical)` | Initialize (memset, so re-initialization works; illegal configurations / one-sided hooks are rejected) |
| | `elib_ts_deinit(ctx)` | Deinitialize (NULL-safe; `running` and `initialized` are cleared) |
| Scheduler | `elib_ts_start(ctx)` | Start; **clears `elapsed_ms` of every active task** |
| | `elib_ts_stop(ctx)` | Stop; **freezes tick accumulation**, and poll becomes a no-op |
| Driving | `elib_ts_tick_update(ctx, ms)` | the tick ISR feeds the elapsed milliseconds (accumulate only) |
| | `elib_ts_poll(ctx)` | main-loop dispatch driver |
| Tasks | `elib_ts_register(ctx, period_ms, timeout_ms, exec_cb, init_cb, anomaly_cb, user_data, out_task_id)` | Register; the ID is the index of the first UNUSED slot |
| | `elib_ts_task_start(ctx, task_id)` | Start a stopped task (elapsed resets to zero; enters INIT when there is an init_cb) |
| | `elib_ts_task_stop(ctx, task_id)` | Stop a task (it is no longer dispatched afterwards) |
| | `elib_ts_task_delete(ctx, task_id)` | Delete a task; the slot returns to UNUSED and can be reused |
| | `elib_ts_task_set_period(ctx, task_id, period_ms)` | Change the period at run time |
| | `elib_ts_task_get_info(ctx, task_id, &info)` | Query a task snapshot |

Error codes (9, `elib_ts_err_t`):

| Error code | Level | Meaning |
|--------|------|------|
| `ELIB_TS_OK` | — | Success |
| `ELIB_TS_ERR_INVALID_PARAM` | General | NULL pointer / `max_tasks == 0` / `period_ms == 0` / one-sided hook |
| `ELIB_TS_ERR_NOT_INITIALIZED` | Scheduler level | not `init`ed, or already `deinit`ed |
| `ELIB_TS_ERR_NOT_FOUND` | Task level | ID out of range, or the slot is UNUSED |
| `ELIB_TS_ERR_FULL` | Task level | the task pool is full |
| `ELIB_TS_ERR_NOT_RUNNING` | Scheduler level | `stop` called while the scheduler is not running |
| `ELIB_TS_ERR_ALREADY_RUNNING` | Scheduler level | the scheduler is already running |
| `ELIB_TS_ERR_TASK_STOPPED` | Task level | `task_stop` on a task that is already STOPPED |
| `ELIB_TS_ERR_TASK_RUNNING` | Task level | `task_start` on a task that is not STOPPED |

> `*_TASK_RUNNING` is a legacy name: its meaning is "the task is currently not STOPPED"; it **does not** mean the task is executing exec_cb.

## State Machine

```
  UNUSED --register(no init_cb)-------------------------> READY
  UNUSED --register(with init_cb)--> INIT --poll runs init_cb--> READY

  READY --elapsed_ms >= period_ms and this poll's turn--> RUNNING
  RUNNING --exec_cb returns (task state is still RUNNING)--> READY

  READY / INIT / RUNNING --task_stop--> STOPPED
  STOPPED --task_start--> INIT (with init_cb) or READY
  any non-UNUSED --task_delete--> UNUSED

            ┌──────────── periodic execution ─────────────┐
            │                                             │
            ▼                                             │
          READY ──(due)──► RUNNING ──(returns)────────────┘
            │                 │
            │task_stop        │task_stop (self-stop inside the callback)
            ▼                 ▼
         STOPPED ◄────────────┘
            │
            │task_start (elapsed resets to zero)
            ▼
      INIT or READY
```

- `register`: no `init_cb` → `READY`; with `init_cb` → `INIT`
- `INIT`: the next poll runs `init_cb` and then moves to `READY` (in the same round it does **not** run `exec_cb` and does not consume the period)
- `READY`: when `elapsed_ms >= period_ms` it becomes `RUNNING` in this round's poll and runs `exec_cb`
- `RUNNING`: exists only while the callback runs (transient); it returns to `READY` once the callback returns
- `task_stop`: `READY` / `INIT` / `RUNNING` → `STOPPED`
- `task_start`: `STOPPED` → `INIT` (with init_cb) or `READY`; `elapsed_ms` resets to zero
- `task_delete`: any non-`UNUSED` → `UNUSED` (the slot can be reused by `register`)
- the scheduler `stop` **does not modify task states**; it only freezes time progression

## Semantic Contract

### Time Driving, Precision and Missed Ticks

`elib_ts_tick_update(ctx, ms)` is called from the tick interrupt and only adds `ms` to the tasks' time accumulators; every scheduling decision happens inside `elib_ts_poll()`.

**Missed-tick semantics**: in a single poll a due task runs at most once; time beyond the period is dropped and the next round starts counting from 0 — a "one call, at most one run" drift. The tick granularity should therefore be far smaller than the shortest task period.

**Execution time precision**: `exec_time_ms` is measured in ticks — ticks that arrive while the callback runs are counted (the task is `RUNNING` at that point). An execution shorter than one tick reports 0. For millisecond resolution you must use a 1 ms tick; to observe finer jitter the only option is a denser tick period.

### Scheduler stop / start

| Operation | Effect |
|------|------|
| `elib_ts_stop` | `running = 0`; `tick_update` no longer accumulates (**all time freezes**); `poll` returns OK immediately and dispatches no task |
| `elib_ts_start` | `running = 1`; clears `elapsed_ms` of every `INIT` / `READY` task, so each active task recounts its period from a known state |

A `STOPPED` task's `elapsed_ms` does not accumulate while it is `task_stop`ped and is cleared when it is `task_start`ed.

### Timeout Detection

```
timeout_ms > 0 && exec_time_ms >= timeout_ms  →  ELIB_TS_ANOMALY_TIMEOUT
```

- `timeout_ms == 0` disables it
- the boundary is `>=`: exactly equal to the threshold also counts as a timeout
- with `anomaly_cb == NULL` only `exec_time_ms` is recorded and no error is reported
- each over-limit execution triggers the anomaly callback once, and the callback sees the updated `exec_time_ms` and the task `user_data`

### Priority and Preemption

The array index is the priority: in one poll round index 0 is dispatched first, then downward in order.

Scheduling is **strictly non-preemptive**: once `exec_cb` starts it is never interrupted by a higher-priority task, and the remaining tasks of the round wait until that callback returns.

**No starvation guarantee**: when a high-priority task takes too long, later tasks are delayed; this is a design boundary rather than a bug — if you need hard real time, do your own period budgeting.

### Callback Reentrancy Contract

Callbacks run in the caller's context of `elib_ts_poll()` (the main loop), **not** in an interrupt, and they **do not hold** the critical section.

Inside a callback you may **only operate on the task itself**:

- Allowed: `task_stop` / `task_start` / `task_set_period` / `task_get_info` on itself
- Forbidden: `register`, deleting other tasks, re-entering `elib_ts_poll`, querying across tasks (undefined behavior)

After `init_cb` / `exec_cb` / `anomaly_cb` return, `poll` always **re-checks the task state**:

- self-stop inside `exec_cb` → stays `STOPPED` and is not "revived" to READY by the scheduler
- self-delete inside `exec_cb` → the slot is cleared to UNUSED straight away and the scheduler no longer writes `exec_time_ms` / `max_exec_time_ms`
- self-stop inside `init_cb` → stays `STOPPED` and is not forced to READY
- stop followed by start inside the callback → the callback decides the state (it lands in `INIT`/`READY`) and the scheduler does not override it

### set_period Rules

`elapsed_ms` is treated as **absolute millisecond progress** and is never rescaled; a new period takes effect from the next evaluation:

| State | Behavior |
|------|------|
| READY / INIT | keeps `elapsed_ms` and compares against the new period |
| RUNNING (changed inside the callback) | written immediately, effective at the next period check |
| STOPPED | may be changed; elapsed resets to zero at `task_start` |

## Concurrency Model (ISR + Main Loop)

`elib_ts_tick_update()` is called from the tick ISR; every other API is called from the main loop. Fields shared across contexts (`elapsed_ms`, `state`, `running`, `initialized`) are declared `volatile`; the library assumes 32-bit-aligned reads and writes are atomic.

State transitions are wrapped in user-supplied critical-section hooks — the hooks are passed to `elib_ts_init()` **in pairs**, and `init` rejects one-sided hooks (only enter or only exit):

```c
/* example: save/restore PRIMASK on Cortex-M */
static uint32_t g_crit_save;

static void crit_enter(void)
{
    g_crit_save = __get_PRIMASK();
    __disable_irq();
}

static void crit_exit(void)
{
    __set_PRIMASK(g_crit_save);
}

elib_ts_init(&g_sched_ctx, g_tasks, MAX_TASKS, crit_enter, crit_exit);
```

- the hooks must mask interrupts in a **save/restore** fashion and must support nesting
- passing `NULL, NULL` declares single-context use (for example when the tick is also software-driven from the main loop)
- the critical section is **not held** while a callback runs, so task APIs may be called safely from callbacks

## Building

Add `include/` to the header search path and compile the source as an ordinary C99 object file:

```bash
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    -c src/elib_ts_core.c -o elib_ts_core.o
```

The source file includes the umbrella header directly (`#include "elib_ts.h"`), depends on no relative paths, and ships no build system of its own.

## Testing

`test/` holds 11 independent test files (64 cases in total); each has its own `main()` and can be compiled and run separately:

```bash
for t in test/test_*.c; do
    gcc -std=c99 -Wall -Wextra -Werror -Iinclude "$t" src/elib_ts_core.c -o "${t%.c}.exe"
    "./${t%.c}.exe"
done
```

| File | Coverage |
|------|------|
| `test_init.c` | init/deinit, rejection of NULL pointers and zero capacity, rejection of one-sided hooks, re-initializing a dirty context, every API rejected after deinit |
| `test_register.c` | parameter validation, ID ordering, pool full, init_cb deciding INIT/READY, a reused slot after delete starting clean, user_data storage |
| `test_scheduler.c` | start/stop state gating, the tick frozen while stopped, start clearing elapsed, poll doing nothing while stopped, balanced critical hooks |
| `test_task_lifecycle.c` | stop/start state gating, start resetting elapsed, delete freeing the slot, rejection of duplicate delete / out-of-range / UNUSED slots, deleting a STOPPED task |
| `test_set_period.c` | period changes across the three states READY/INIT/RUNNING, elapsed never rescaled, changing it inside the callback, zero period and illegal ID |
| `test_tick.c` | accumulation for READY/INIT, skipping STOPPED/UNUSED, UINT32_MAX saturation, tick(0)/tick(NULL)/no-op after deinit |
| `test_poll_dispatch.c` | no dispatch before due, exactly one run at `elapsed == period`, no catch-up after a large overrun, dispatch in array order, STOPPED skipped, INIT runs first and then waits a full period |
| `test_exec_time.c` | ticks during the callback counted as execution time, less than one tick reports 0, max only rises, elapsed keeps wall-clock progress, the info snapshot exposes the timings |
| `test_timeout.c` | triggered above the threshold, the `>=` boundary, silent below the threshold, 0 disabling it, NULL callback safe, user_data passed through, one report per over-limit run |
| `test_get_info.c` | initial snapshot, snapshot after execution, STOPPED state, rejection for bad parameters / out-of-range / UNUSED / after deinit |
| `test_reentrancy.c` | self-stop inside exec is not revived, self-delete leaves no accounting behind and the slot can be reused, self-stop inside init is not forced to READY, stop followed by start, user_data delivered to both callbacks |

Behavior consistency is additionally cross-checked against an independent Python reference model (28 scenarios / 82 assertions, 0 failures); the model was rewritten from the umbrella-header contract without reusing the C code structure.

## License

MIT License (see `LICENSE`).
