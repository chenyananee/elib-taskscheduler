# elib-taskscheduler

资源受限嵌入式系统用的轻量任务调度器：零动态内存、零全局变量、纯 C99，单头伞 API。

当前版本：**2.0.0**（`ELIB_TS_VERSION`），单头 `include/elib_ts.h`。

## 特性

- **零动态内存 / 零全局**：全部状态由用户静态提供的 `ctx + 任务数组` 承载
- **单头伞 API**：全部公共类型与函数集中在 `elib_ts.h`，无子头
- **ISR / 主循环分工**：tick 中断只做毫秒累加，`elib_ts_poll()` 在主循环里派发，回调绝不在中断里执行
- **数组即优先级**：下标即派发顺序（index 0 最先），严格非抢占
- **可选初始化回调**：`init_cb` 先跑一次，之后才进入周期执行
- **执行耗时测量**：`exec_time_ms` / `max_exec_time_ms`（tick 量化）
- **超时异常检测**：`timeout_ms` 阈值触发 `ELIB_TS_ANOMALY_TIMEOUT`
- **user_data 透传**：init / exec / anomaly 三个回调都拿到用户指针
- **可选临界区钩子**：`init` 拒收单边钩子（必须成对）
- **纯 C99 实现**，C++ 兼容（`extern "C"`）

## 快速开始

### 1. 静态定义上下文与任务池

```c
#include "elib_ts.h"   /* 构建时加 -I<repo>/include */

#define MAX_TASKS 4

static elib_ts_ctx_t  g_sched_ctx;
static elib_ts_task_t g_tasks[MAX_TASKS];

static void app_init(void)
{
    /* 无临界钩子：声明单上下文使用 */
    elib_ts_init(&g_sched_ctx, g_tasks, MAX_TASKS, NULL, NULL);
}
```

### 2. 注册任务并启动调度器

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
                     50u,             /* timeout_ms，0 表示禁用 */
                     led_toggle,      /* exec_cb，必填 */
                     led_init,        /* init_cb，可 NULL */
                     NULL,            /* anomaly_cb，可 NULL */
                     &g_led,          /* user_data  */
                     &g_led_task_id);

    elib_ts_start(&g_sched_ctx);      /* 清空所有活动任务的 elapsed */
}
```

### 3. 中断喂 tick，主循环派发

```c
/* 1ms tick 中断：只累加，不执行任何用户代码 */
void SysTick_Handler(void)
{
    elib_ts_tick_update(&g_sched_ctx, 1u);
}

/* 主循环 */
for (;;) {
    elib_ts_poll(&g_sched_ctx);
    /* ... 其他后台逻辑 ... */
}
```

## API 一览

| 分组 | 函数 | 说明 |
|------|------|------|
| 生命周期 | `elib_ts_init(ctx, tasks, max_tasks, enter_critical, exit_critical)` | 初始化（memset 覆盖重初始化；非法配置/单边钩子拒收） |
| | `elib_ts_deinit(ctx)` | 反初始化（NULL 安全；`running` 与 `initialized` 清零） |
| 调度器 | `elib_ts_start(ctx)` | 启动；**清零所有活动任务的 `elapsed_ms`** |
| | `elib_ts_stop(ctx)` | 停止；**冻结 tick 累加**，poll 变为空操作 |
| 驱动 | `elib_ts_tick_update(ctx, ms)` | tick ISR 喂入经过毫秒数（只累加） |
| | `elib_ts_poll(ctx)` | 主循环派发驱动 |
| 任务 | `elib_ts_register(ctx, period_ms, timeout_ms, exec_cb, init_cb, anomaly_cb, user_data, out_task_id)` | 注册；ID = 首个 UNUSED 槽位下标 |
| | `elib_ts_task_start(ctx, task_id)` | 启动已停止的任务（elapsed 归零；有 init_cb 则进入 INIT） |
| | `elib_ts_task_stop(ctx, task_id)` | 停止任务（此后不再被派发） |
| | `elib_ts_task_delete(ctx, task_id)` | 删除任务，槽位回归 UNUSED 可复用 |
| | `elib_ts_task_set_period(ctx, task_id, period_ms)` | 运行中改周期 |
| | `elib_ts_task_get_info(ctx, task_id, &info)` | 查询任务快照 |

错误码（9 个，`elib_ts_err_t`）：

| 错误码 | 层级 | 含义 |
|--------|------|------|
| `ELIB_TS_OK` | — | 成功 |
| `ELIB_TS_ERR_INVALID_PARAM` | 通用 | 空指针 / `max_tasks == 0` / `period_ms == 0` / 单边钩子 |
| `ELIB_TS_ERR_NOT_INITIALIZED` | 调度器级 | 未 init，或已 deinit |
| `ELIB_TS_ERR_NOT_FOUND` | 任务级 | ID 越界，或槽位为 UNUSED |
| `ELIB_TS_ERR_FULL` | 任务级 | 任务池已满 |
| `ELIB_TS_ERR_NOT_RUNNING` | 调度器级 | 调度器未启动时调用 `stop` |
| `ELIB_TS_ERR_ALREADY_RUNNING` | 调度器级 | 调度器已在运行 |
| `ELIB_TS_ERR_TASK_STOPPED` | 任务级 | 任务已经是 STOPPED 又 `task_stop` |
| `ELIB_TS_ERR_TASK_RUNNING` | 任务级 | 任务不在 STOPPED 又 `task_start` |

> `*_TASK_RUNNING` 是历史命名：它的语义是"该任务当前不是 STOPPED"，**不代表**任务正在执行 exec_cb。

## 状态机

```
  UNUSED --register(无 init_cb)---------------------------> READY
  UNUSED --register(有 init_cb)--> INIT --poll 执行 init_cb--> READY

  READY --elapsed_ms >= period_ms 且本轮 poll 轮到--> RUNNING
  RUNNING --exec_cb 返回（任务状态仍为 RUNNING）--> READY

  READY / INIT / RUNNING --task_stop--> STOPPED
  STOPPED --task_start--> INIT（有 init_cb）或 READY
  任意非 UNUSED --task_delete--> UNUSED

            ┌──────────── 周期执行 ────────────┐
            │                                 │
            ▼                                 │
          READY ──(到期)──► RUNNING ──(返回)───┘
            │                 │
            │task_stop        │task_stop（回调内自停）
            ▼                 ▼
         STOPPED ◄────────────┘
            │
            │task_start（elapsed 归零）
            ▼
      INIT 或 READY
```

- `register`：无 `init_cb` → `READY`；有 `init_cb` → `INIT`
- `INIT`：下一次 poll 执行 `init_cb`，随后转 `READY`（同一轮**不**执行 `exec_cb`，也不消耗周期）
- `READY`：`elapsed_ms >= period_ms` 时在本轮 poll 中转为 `RUNNING` 并执行 `exec_cb`
- `RUNNING`：仅存在于回调执行期间（瞬态）；回调返回后回 `READY`
- `task_stop`：`READY` / `INIT` / `RUNNING` → `STOPPED`
- `task_start`：`STOPPED` → `INIT`（有 init_cb）或 `READY`；`elapsed_ms` 归零
- `task_delete`：任意非 `UNUSED` → `UNUSED`（槽位可被 `register` 复用）
- 调度器 `stop` **不修改任务状态**，只冻结时间推进

## 语义契约

### 时间驱动、精度与丢拍

`elib_ts_tick_update(ctx, ms)` 由 tick 中断调用，只把 `ms` 累加到任务的时间累加器上；调度判定全部发生在 `elib_ts_poll()` 里。

**丢拍语义**：单次 poll 中，一个到期任务最多执行一次；超出周期的多余时间被丢弃，下一轮从 0 重新计时——即"调用一次、最多执行一次"的漂移。因此 tick 粒度应远小于最小任务周期。

**执行耗时精度**：`exec_time_ms` 以 tick 为单位计量——回调执行期间到达的 tick 被计入（此时任务处于 `RUNNING`）。短于一个 tick 的执行报 0。想得到毫秒级分辨率，就必须用 1ms tick；想监控更细的抖动，只能加密 tick 周期。

### 调度器 stop / start

| 操作 | 效果 |
|------|------|
| `elib_ts_stop` | `running = 0`；`tick_update` 不再累加（**所有时间冻结**）；`poll` 直接返回 OK，不派发任何任务 |
| `elib_ts_start` | `running = 1`；把所有 `INIT` / `READY` 任务的 `elapsed_ms` 清零，使每个活动任务从已知状态重新计周期 |

`STOPPED` 任务的 `elapsed_ms` 在 `task_stop` 期间不累加，在其 `task_start` 时清零。

### 超时判定

```
timeout_ms > 0 && exec_time_ms >= timeout_ms  →  ELIB_TS_ANOMALY_TIMEOUT
```

- `timeout_ms == 0` 表示禁用
- 边界取 `>=`：恰好等于阈值也判定超时
- `anomaly_cb == NULL` 时只记录 `exec_time_ms`，不报错
- 每次超限执行触发一次异常回调，回调可见已更新的 `exec_time_ms` 与任务 `user_data`

### 优先级与抢占

数组下标即优先级：同一轮 poll 中 index 0 最先派发，依次向下。

调度**严格非抢占**：`exec_cb` 一旦开始执行就不会被更高优先级的任务打断，本轮其余任务顺延到该回调返回之后。

**不提供饥饿保证**：高优先级任务耗时过长时，后续任务会被推迟；这不是 bug 而是设计边界，需要硬实时请自行做周期预算。

### 回调重入契约

回调运行在 `elib_ts_poll()` 的调用方上下文（主循环），**不在**中断里，也**不持有**临界区。

回调内**只允许操作自己**：

- 允许：对自己 `task_stop` / `task_start` / `task_set_period` / `task_get_info`
- 禁止：`register`、删除其他任务、重入 `elib_ts_poll`、跨任务查询（未定义行为）

`poll` 在 `init_cb` / `exec_cb` / `anomaly_cb` 返回后都会**复查任务状态**：

- `exec_cb` 内自停 → 保持 `STOPPED`，不会被调度器"复活"成 READY
- `exec_cb` 内自删 → 槽位直接清空为 UNUSED，调度器不再写入 `exec_time_ms` / `max_exec_time_ms`
- `init_cb` 内自停 → 保持 `STOPPED`，不会被强行改成 READY
- 自停后又自启 → 状态由回调自己决定（落到 `INIT`/`READY`），调度器不覆盖

### set_period 规则

`elapsed_ms` 视为**绝对毫秒进度**，不做比例缩放；新周期从下一次判定开始生效：

| 状态 | 行为 |
|------|------|
| READY / INIT | 保留 `elapsed_ms`，按新周期比较 |
| RUNNING（回调内自改） | 立即写入，下一次判周期时生效 |
| STOPPED | 允许修改，`task_start` 时 elapsed 归零 |

## 并发模型（ISR + 主循环）

`elib_ts_tick_update()` 供 tick ISR 调用，其余 API 供主循环调用。跨上下文共享的字段（`elapsed_ms`、`state`、`running`、`initialized`）声明为 `volatile`；库假设 32 位对齐的读写是原子的。

状态跃迁段由用户提供的临界区钩子包裹——钩子通过 `elib_ts_init()` **成对**传入，`init` 会拒收单边钩子（只给 enter 或只给 exit）：

```c
/* 例：Cortex-M 下用 PRIMASK 保存/恢复 */
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

- 钩子必须**保存/恢复**式中断屏蔽，且支持嵌套
- 传 `NULL, NULL` 即声明单上下文使用（例如 tick 也在主循环里软件驱动）
- 回调执行期间**不持有**临界区，因此回调里可以安全调用任务 API

## 编译

将 `include/` 加入头文件搜索路径，源文件编译为普通 C99 目标文件：

```bash
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    -c src/elib_ts_core.c -o elib_ts_core.o
```

源文件直连伞头（`#include "elib_ts.h"`），不依赖任何相对路径，无随库构建系统。

## 测试

`test/` 下 11 个独立测试文件（共 64 个用例），每件自带 `main()`，可分别编译运行：

```bash
for t in test/test_*.c; do
    gcc -std=c99 -Wall -Wextra -Werror -Iinclude "$t" src/elib_ts_core.c -o "${t%.c}.exe"
    "./${t%.c}.exe"
done
```

| 文件 | 覆盖 |
|------|------|
| `test_init.c` | init/deinit、空指针与零容量拒收、单边钩子拒收、脏上下文重初始化、deinit 后全 API 拒收 |
| `test_register.c` | 参数校验、ID 顺序、池满、init_cb 判定 INIT/READY、删除后槽位复用净身、user_data 保存 |
| `test_scheduler.c` | start/stop 状态门禁、停止时 tick 冻结、start 清零 elapsed、停止时 poll 空转、临界钩子配平 |
| `test_task_lifecycle.c` | stop/start 状态门禁、start 重置 elapsed、delete 释放槽位、重复删除/越界/UNUSED 槽位拒收、删除 STOPPED 任务 |
| `test_set_period.c` | READY/INIT/RUNNING 三态改周期、elapsed 不缩放、回调内自改、零周期与非法 ID |
| `test_tick.c` | READY/INIT 累加、跳过 STOPPED/UNUSED、UINT32_MAX 饱和、tick(0)/tick(NULL)/deinit 后空操作 |
| `test_poll_dispatch.c` | 未到期不派发、`elapsed == period` 恰好一次、大幅超时不补跑、数组顺序派发、STOPPED 跳过、INIT 先跑再等整周期 |
| `test_exec_time.c` | 回调内 tick 计入耗时、不足一 tick 报 0、max 只升不降、elapsed 保留墙钟进度、info 暴露耗时 |
| `test_timeout.c` | 超阈值触发、`>=` 边界、低于阈值静默、0 禁用、NULL 回调安全、user_data 透传、每次超限各报一次 |
| `test_get_info.c` | 初始快照、执行后快照、STOPPED 状态、错误参数/越界/UNUSED/deinit 拒收 |
| `test_reentrancy.c` | exec 内自停不被复活、自删无残留记账且槽位可复用、init 内自停不被强制 READY、自停后自启、user_data 送达两个回调 |

行为一致性另用独立 Python 参考模型交叉验证（28 个场景 / 82 项断言，0 失败），模型按伞头契约独立重写，不复用 C 代码结构。

## 许可证

MIT License（见 `LICENSE`）。
