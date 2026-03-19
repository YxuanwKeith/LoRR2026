## LoRR2026 技术约束笔记

> 文档定位：时间预算、运行限制、系统行为细节与调参注意项。

这份说明以**当前仓库源码实际行为**为准，主要依据 `driver.cpp`、`CompetitionSystem.cpp`、`Entry.cpp`、`TaskScheduler.cpp`、`MAPFPlanner.cpp`、`Simulator.cpp`、`Executor.cpp` 与 `default_planner/const.h`。

### 1. 总览表

> 说明：下面把“真正的时间预算”和“与时间相关的停止条件/派生量”都放在一起，便于看全局。

| 环节 | 参数 / 预算来源 | 默认值 | 单位 | 实际作用 | 超时 / 到点后的行为 |
| --- | --- | ---: | --- | --- | --- |
| 仿真总长度 | `simulationTime` | `5000` | tick | 控制主循环运行到多少个执行步 | `curr_timestep >= simulationTime` 后结束；**它不是毫秒级时间预算** |
| 预处理总预算 | `preprocessTimeLimit` | `30000` | ms | 覆盖 `Entry::initialize(...)` 与执行器初始化阶段 | 若 `Entry::initialize(...)` 在 deadline 前未完成，或初始化耗时超过预算，则记录 preprocessing timeout 并 `_exit(124)` |
| 初始规划预算 | `initialPlanTimeLimit` | `1000` | ms | 第一次 `Entry::compute(...)` 的总预算 | 若首轮未按时完成，系统不会停；主线程继续让所有 agent 按 `move` 循环等待，直到 planner 真正返回 |
| 周期重规划预算 | `planCommTimeLimit` | `1000` | ms | 后续每轮 `Entry::compute(...)` 的总预算 | 若到点还没返回，只记 `planner timeout`，系统继续执行旧的 `staged_actions` / 等待，直到 planner 返回 |
| 最小通信间隔 | `planCommTimeLimit` | `1000` | ms | 限制两次接入新规划之间的最小间隔 | 到点且旧 planner 已结束后，才会 `process_new_plan(...)` 并启动下一轮 `compute(...)` |
| 单个执行 tick 预算 | `actionMoveTimeLimit` | `100` | ms | 每次 `Simulator::move(...)` / `Executor::next_command(...)` 的预算 | 若 `next_command(...)` 超时，系统会插入若干个全体 `wait` 的 tick 进行补偿 |
| 新计划接入预算（接口层） | `executorProcessPlanTimeLimit` | `100` | ms | 传入 `Simulator::process_new_plan(sync_time_limit, ...)`，再转给 `Executor::process_new_plan(...)` | **当前默认实现里没有真正拿它做超时判定**；见下文“实现细节警告” |
| 新计划接入实际超时判定 | `actionMoveTimeLimit` | `100` | ms | `Simulator::process_new_plan(...)` 里当前用 `overtime_runtime` 计算超时 | 处理新计划太久时，会按 `actionMoveTimeLimit` 为粒度插入额外的 `move(...)` / `wait` |

### 2. 各环节详细说明

### 2.1 预处理阶段

- 入口：`BaseSystem::initialize()`。
- 外层总预算：`preprocessTimeLimit`。
- 这阶段主要做三件事：
  - 调 `Entry::initialize(preprocess_time_limit)`；
  - 调 `simulator.initialise_executor(preprocess_time_limit)`；
  - 若超时则直接退出程序。

关键点：

- `Entry::initialize(...)` 会负责规划侧初始化；
- `Simulator` 也会在这个阶段初始化 `Executor`；
- 从系统视角看，只需要把它理解成：**整段预处理受 `preprocessTimeLimit` 约束**。

### 2.2 初始规划阶段

- 入口：`BaseSystem::simulate()` 开始后的第一次 `planner_wrapper()`。
- 外层预算：`initialPlanTimeLimit`。
- 实际调用链：
  - `planner_wrapper()`
  - `Entry::compute(time_limit, proposed_plan, proposed_schedule)`
  - `TaskScheduler::plan(...)`
  - `MAPFPlanner::plan(...)`

关键点：

- 这份 `time_limit` 是**整轮 compute 的总预算**，不是只给路径规划器。
- 如果初始规划在 `initialPlanTimeLimit` 内没返回，系统不会崩；它会持续执行 `simulator.move(actionMoveTimeLimit)`，相当于全体先等着，直到 planner 真正结束。

### 2.3 周期重规划阶段

- 后续每轮 planner 的外层预算直接等于 `planCommTimeLimit`。
- 同一个参数还承担**最小通信间隔**的作用。

更准确地说：

- `planCommTimeLimit` **不是**“每个 tick 决定一次 go/stop”；
- 它是“**两次接入新规划之间至少隔多久**”的慢环约束；
- 真正每个 tick 的 `GO/STOP` 决策，是 `Executor::next_command(...)` 在快环里做的。

### 2.4 `compute(...)` 的系统层理解

`Entry::compute(...)` 内部顺序是：

1. `scheduler->plan(time_limit, proposed_schedule)`
2. `update_goal_locations(proposed_schedule)`
3. `planner->plan(time_limit, plan)`

从系统层视角看，关键只需要记住：

- `Entry::compute(...)` 接收的是一轮重规划的**总预算**；
- 这轮预算对应：
  - 初始阶段用 `initialPlanTimeLimit`
  - 周期阶段通常用 `planCommTimeLimit`
- 它先产出任务分配，再产出动作计划。

### 2.5 单步执行阶段

- 入口：`Simulator::move(move_time_limit)`。
- 外层预算：`actionMoveTimeLimit`。
- 它会先调用：
  - `executor->next_command(move_time_limit, agent_command)`
- 然后根据 `GO/STOP` 决策，把动作转换成实际 `Action`，再交给 `ActionModel` 执行。

如果执行器决策超时：

- `Simulator::move(...)` 会比较 `next_command(...)` 的耗时和 `move_time_limit`；
- 若超出预算，就插入若干个全体 `wait` 的 tick；
- 所以这个预算是**真正会影响仿真推进速度和等待行为**的。

### 2.6 新计划接入阶段

- 入口：`Simulator::process_new_plan(sync_time_limit, overtime_runtime, plan)`。
- `BaseSystem::simulate()` 当前调用方式是：
  - `simulator.process_new_plan(process_new_plan_time_limit, simulator_time_limit, proposed_plan)`

按接口命名看：

- `sync_time_limit` 应该对应 `executorProcessPlanTimeLimit`；
- `overtime_runtime` 对应 `actionMoveTimeLimit`。

但当前默认实现有一个要特别注意的地方：

- `Executor::process_new_plan(sync_time_limit, ...)` 的默认实现**没有使用** `sync_time_limit` 做超时判定；
- `Simulator::process_new_plan(...)` 里计算 `diff` 时，用的是 `overtime_runtime`，也就是 `actionMoveTimeLimit`；
- 所以当前代码里，**新计划接入的实际“超时惩罚粒度”由 `actionMoveTimeLimit` 决定，而不是 `executorProcessPlanTimeLimit`**。

这意味着：

- `executorProcessPlanTimeLimit` 在接口设计上存在；
- 但在当前 starter-kit 默认实现里，它更像是“预留接口参数”，不是严格生效的硬限制。

### 3. 与时间限制相关的派生量

这些不是直接的 CLI 时间参数，但由系统层时间参数推导，实际会影响 planner / executor 行为。

### 3.1 默认 planner 的最小输出步数

`MAPFPlanner::plan(...)` 中：

\[
min\_plan\_steps = \left\lfloor \frac{planCommTimeLimit}{actionMoveTimeLimit \times maxCounter} \right\rfloor + 1
\]

含义：

- 默认 planner 会尽量产出至少这么多步动作，覆盖下一次规划通信回来之前的一段执行窗口。

### 3.2 Executor 的窗口大小

`Executor::initialize(...)` 中：

\[
window\_size = \max\left(1, \left\lfloor \frac{planCommTimeLimit}{actionMoveTimeLimit \times maxCounter} \right\rfloor \right)
\]

但当前默认 `Executor::process_new_plan(...)` 的实现实际上是：

- 按 `plan[0].size()` 把整版计划全部 append 进 `staged_actions`；
- **没有真的按 `window_size` 截断**。

所以：

- `window_size` 目前更像是“设计意图”；
- 当前活跃实现里，真正接入的是整版计划。

### 4. 推荐理解方式

如果只想记住最核心的几条，可以记下面这组：

- **`preprocessTimeLimit`**：离线初始化总预算，超时会直接退出；
- **`initialPlanTimeLimit`**：第一次 `compute(...)` 的预算；
- **`planCommTimeLimit`**：后续每轮 `compute(...)` 的预算，同时也是慢环重规划的最小通信间隔；
- **`actionMoveTimeLimit`**：每个执行 tick 的预算，超时会导致插入 `wait`；
- **`executorProcessPlanTimeLimit`**：接口上是“新计划接入预算”，但当前默认实现里并未真正成为硬超时判断；
- **`simulationTime`**：仿真总 tick 数，不是毫秒级预算。

### 5. 仓库内公开测试/示例数据的地图规模

下面的统计基于当前仓库 `example_problems` 中可见的公开输入文件，而**不是**隐藏评测集。

- **样例数量**：6 组输入
- **地图文件数量**：6 个输入对应 6 次引用，落在 5 种不同尺寸上
- **最小地图**：`32 x 32`（`random-32-32-20.map`）
- **最大地图**：`481 x 530`（`brc202d.map`）

| 域 / 输入文件 | 地图文件 | 地图规模 |
| --- | --- | --- |
| `random.domain/random_32_32_20_100.json` | `random-32-32-20.map` | `32 x 32` |
| `warehouse.domain/warehouse_small_200.json` | `warehouse_small.map` | `33 x 57` |
| `warehouse.domain/warehouse_large_5000.json` | `warehouse_large.map` | `140 x 500` |
| `warehouse.domain/sortation_large_2000.json` | `sortation_large.map` | `140 x 500` |
| `city.domain/paris_1_256_250.json` | `Paris_1_256.map` | `256 x 256` |
| `game.domain/brc202d_500.json` | `brc202d.map` | `481 x 530` |

如果只看**不同尺寸**，当前公开样例覆盖的是：

- `32 x 32`
- `33 x 57`
- `140 x 500`
- `256 x 256`
- `481 x 530`

### 6. 一个容易误解但很重要的结论

这套系统里，存在两条时间尺度：

- **慢环**：`planCommTimeLimit` 控制的重规划 / 通信节奏；
- **快环**：`actionMoveTimeLimit` 控制的每 tick 执行节奏。

不要把它们混成一回事：

- `planCommTimeLimit` **不负责**每步 `GO/STOP`；
- 每步 `GO/STOP` 是 `Executor::next_command(...)` 负责；
- `planCommTimeLimit` 负责“**多久才能接入一版新规划并启动下一轮 compute**”。
