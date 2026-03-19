# LoRR2026 本地开发骨架

这份文档只讨论一个前提：

- 评测会覆盖 `Evaluation_Environment.md` 中列出的受保护文件；
- 因此本地长期可维护的实现，必须把核心自定义逻辑放在**不会被覆盖的位置**，并通过允许修改的 API 接入。

## 1. 目标

如果希望三条赛道都能做，并最终沉淀成一套完整方案，本地建议拆成四层：

1. `Entry` 层负责总编排。
2. `Scheduler` 层负责任务分配与重分配策略。
3. `Planner` 层负责多步动作生成。
4. `Executor` 层负责计划接入与每 tick 放行。

但真正可提交的版本，必须把逻辑落在允许修改的文件或新增文件中，再由允许修改的入口类调用。

## 2. 推荐代码放置方式

### 2.1 Scheduler 相关

推荐新增目录或文件：

- `custom_scheduler/`
- 或 `src/custom_scheduler/`

建议拆分：

- `assignment_cost.{h,cpp}`
- `region_balance.{h,cpp}`
- `scheduler_policy.{h,cpp}`

入口落点：

- `src/TaskScheduler.cpp`
  - 只保留很薄的一层适配，调用你新增的 scheduler 模块。

### 2.2 Planner 相关

推荐新增目录或文件：

- `custom_planner/`
- 或 `src/custom_planner/`

建议拆分：

- `planner_policy.{h,cpp}`
- `goal_selector.{h,cpp}`
- `reservation_table.{h,cpp}`
- `rolling_horizon.{h,cpp}`

入口落点：

- `src/MAPFPlanner.cpp`
  - 只负责时间预算换算与调用新增 planner 模块。

### 2.3 Executor 相关

推荐新增目录或文件：

- `custom_executor/`
- 或 `src/custom_executor/`

建议拆分：

- `plan_adapter.{h,cpp}`
- `release_policy.{h,cpp}`
- `delay_guard.{h,cpp}`
- `dependency_graph.{h,cpp}`

入口落点：

- `src/Executor.cpp`
  - 只负责把系统接口转给自定义执行模块。

### 2.4 Entry 总编排

如果做 combined：

- `src/Entry.cpp` 负责统一串接 scheduler / planner。
- 可以新增 `src/entry_pipeline.{h,cpp}` 管理共用流程，例如：
  - 何时刷新内部缓存
  - 何时触发重分配
  - 何时把调度结果转成 goal set

## 3. 建议的数据层骨架

为了避免逻辑分散，建议新增一个独立的数据快照层，例如：

- `src/core/`

可包含：

- `env_snapshot.{h,cpp}`
  - 从 `SharedEnvironment` 抽取算法真正需要的只读视图。
- `task_view.{h,cpp}`
  - 提供任务状态、可分配性、链式 errands 访问接口。
- `agent_view.{h,cpp}`
  - 提供 agent 当前状态、目标、局部拥堵指标。
- `map_cache.{h,cpp}`
  - 管理距离表、区域划分、热点统计、瓶颈位置等。

这样做的价值是：

- planner / scheduler / executor 共享同一套派生数据；
- 以后替换策略时，不需要反复直接读 `SharedEnvironment` 的原始字段。

## 4. 三赛道统一实现思路

### 4.1 Scheduling Track

最小骨架：

- 新增任务评分器
- 新增区域负载均衡
- 新增任务重分配规则
- 由 `TaskScheduler` 调用

### 4.2 Execution Track

最小骨架：

- 新增 plan-to-stage 适配器
- 新增每 tick 放行策略
- 新增 delay-aware 兜底逻辑
- 由 `Executor` 调用

### 4.3 Combined Track

完整骨架：

- Scheduler 决定接什么任务
- Planner 决定未来一段路怎么走
- Executor 决定这一 tick 能不能走
- Entry 统一协调这些模块

## 5. 当前仓库下的位置判断

结合当前仓库状态：

- `default_planner/` 里的原有文件大多在受保护清单中。
- 因此**不应该继续把新增核心实现塞进现有 `default_planner/*.cpp` 里**。
- 如果一定要放在 `default_planner/` 下，也只能新增独立文件，并确认不会依赖“修改受保护默认实现”才能生效。

更稳妥的做法是：

- 把新增代码放到新的目录，如 `src/custom_*` 或 `src/core`；
- 由允许修改的 API 实现文件去调用这些新增模块。

## 6. 下一步落地建议

推荐先补这几个目录骨架：

- `src/core/`
- `src/custom_scheduler/`
- `src/custom_planner/`
- `src/custom_executor/`

然后先做两个最小闭环：

1. 调度闭环：任务评分 + 分配输出。
2. 执行闭环：plan 接入 + GO/STOP 放行。

最后再把 planner 的滚动规划替换进去。
