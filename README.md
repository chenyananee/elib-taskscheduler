# elib-taskscheduler

嵌入式任务调度器 | Embedded Task Scheduler

零动态分配的轻量级任务调度库，专为资源受限的嵌入式系统设计。中断侧维护 tick 累加，主循环侧驱动调度，支持任务优先级、初始化回调、执行耗时跟踪和超时异常检测。

## Features

- 零动态内存分配，所有资源由用户分配
- ISR tick 驱动 + 主循环 poll 调度
- 数组索引优先级（index 0 = 最高优先级）
- 可选初始化回调（init → 下一轮 poll 执行 exec）
- 执行耗时测量与历史最长耗时跟踪
- 超时阈值检测与异常回调
- 可选临界区接口（线程安全）
- 纯 C 实现，C++ 兼容

## Quick Start

```c
#include "elib_ts.h"

/* 用户分配资源 */
elib_ts_ctx_t ctx;
elib_ts_task_t tasks[4];

void my_task(void) {
    /* 周期执行的任务 */
}

void critical_enter(void) { __disable_irq(); }
void critical_exit(void)  { __enable_irq(); }

int main(void) {
    /* 初始化调度器 */
    elib_ts_init(&ctx, tasks, 4, critical_enter, critical_exit);

    /* 注册任务：周期 100ms，超时 50ms */
    uint32_t task_id;
    elib_ts_register(&ctx, 100, 50, my_task, NULL, NULL, &task_id);

    /* 启动调度器 */
    elib_ts_start(&ctx);

    /* 主循环 */
    while (1) {
        elib_ts_poll(&ctx);
    }
}

/* 定时中断中调用（如 SysTick 1ms） */
void SysTick_Handler(void) {
    elib_ts_tick_update(&ctx, 1);
}
```

## API Reference

### 初始化 / 反初始化

| API | 说明 |
|-----|------|
| `elib_ts_init(ctx, tasks, max_tasks, enter_critical, exit_critical)` | 初始化调度器 |
| `elib_ts_deinit(ctx)` | 反初始化 |

### 调度器控制

| API | 说明 |
|-----|------|
| `elib_ts_start(ctx)` | 启动调度器 |
| `elib_ts_stop(ctx)` | 停止调度器 |
| `elib_ts_tick_update(ctx, ms)` | ISR 中调用，累加任务 elapsed_ms |
| `elib_ts_poll(ctx)` | 主循环调度驱动 |

### 任务管理

| API | 说明 |
|-----|------|
| `elib_ts_register(ctx, period_ms, timeout_ms, exec_cb, init_cb, anomaly_cb, &id)` | 注册任务 |
| `elib_ts_task_start(ctx, task_id)` | 启动任务 |
| `elib_ts_task_stop(ctx, task_id)` | 停止任务 |
| `elib_ts_task_set_period(ctx, task_id, period_ms)` | 动态修改周期 |
| `elib_ts_task_get_info(ctx, task_id, &info)` | 查询任务信息 |

### 任务状态

```
UNUSED → STOPPED → INIT → READY ↔ RUNNING
```

- 注册时：有 init_cb → INIT，无 init_cb → READY
- INIT：poll 执行 init_cb 后转为 READY，本轮不执行 exec
- READY：elapsed_ms >= period_ms 时执行 exec_cb

## Build & Test

```bash
gcc -o test/test_elib_ts test/test_elib_ts.c src/elib_ts_core.c -Iinclude
./test/test_elib_ts
```

## License

MIT License, Copyright (c) 2026 ChenYanan
