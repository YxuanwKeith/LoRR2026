### LoRR2026 代码框架笔记

> 文档定位：starter-kit 调用链、关键类、关键函数与默认模块结构索引。

这份文档面向**读 starter-kit 源码、定位关键入口、准备改算法**的场景，重点整理：

- **整体运行框架**：程序从哪里启动，哪些模块在驱动仿真；
- **核心数据流**：任务、规划、执行是怎么串起来的；
- **关键类 / 关键函数**：每个文件里最值得先看的函数是什么；
- **默认算法框架**：当前默认调度器和规划器实际上在做什么；
- **改算法时优先改哪里**：如果想提升成绩，优先动哪些点。

---

## 1. 先看整体：这套系统由哪几层组成

可以把整个系统分成五层：

1. **入口层**：`src/driver.cpp`
   - 读取命令行参数和输入 JSON；
   - 构造 `Entry`、`Executor`、`BaseSystem`；
   - 设置 delay、时间限制、输出路径；
   - 启动仿真并保存结果。

2. **系统编排层**：`src/CompetitionSystem.cpp` + `inc/CompetitionSystem.h`
   - 负责整个比赛系统主循环；
   - 控制什么时候启动 `compute(...)`；
   - 控制什么时候把新计划接入执行；
   - 控制每个 execution tick 的推进；
   - 控制任务完成、补任务、日志和结果输出。

3. **算法入口层**：`src/Entry.cpp` + `inc/Entry.h`
   - 是选手算法的总入口；
   - 内部再拆成：
     - `TaskScheduler`：任务分配；
     - `MAPFPlanner`：多智能体路径规划。

4. **执行仿真层**：`src/Simulator.cpp` + `src/Executor.cpp`
   - `Simulator` 负责推进真实世界状态；
   - `Executor` 负责把 planner 的多步计划变成当前 tick 的实时放行决策。

5. **默认算法层**：`default_planner/`
   - `scheduler.cpp`：默认任务分配；
   - `planner.cpp`：默认多步规划主逻辑；
   - `pibt.cpp`：局部冲突消解与优先级继承；
   - `heuristics.cpp`：距离启发式与邻接关系；
   - `search.cpp`：基于 flow 的 A*；
   - `flow.cpp`：guide path 的流量优化与重规划；
   - `TrajLNS.h`：默认规划器共享状态容器。

---

## 2. 程序的主调用链

### 2.1 启动阶段

入口在 `src/driver.cpp`：

- 读输入 JSON；
- 读地图、起点、任务、delay 配置；
- 构造：
  - `Entry* planner`
  - `Executor* executor`
  - `BaseSystem system`
- 设置关键时间参数：
  - `preprocessTimeLimit`
  - `initialPlanTimeLimit`
  - `planCommTimeLimit`
  - `actionMoveTimeLimit`
  - `executorProcessPlanTimeLimit`
- 调用：`system_ptr->simulate(simulationTime, 100)`。

### 2.2 主循环结构

系统主循环在 `BaseSystem::simulate(...)` 中，大致顺序是：

1. `initialize()`
   - 初始化 `SharedEnvironment`
   - 初始化 planner / executor
   - 初始 reveal 任务
2. 启动第一次 planner 线程
3. 进入主循环，直到 `curr_timestep >= simulationTime`
4. 在循环中不断执行三件事：
   - **如果上一轮规划完成且通信窗口到点**：接入新计划并启动下一轮 `compute(...)`
   - **每个 tick**：调用 `simulator.move(...)`
   - **每个 tick 后**：调用 `task_manager.update_tasks(...)`

所以系统的节奏不是“每一步都重新全局规划”，而是：

- **慢环**：周期性运行 `Entry::compute(...)`
- **快环**：每个 tick 用 `Executor + Simulator` 推进执行
- **任务层**：每个 tick 检查任务完成并补任务

---

## 3. 三条核心数据流

### 3.1 任务流

任务流由 `TaskManager` 管：

- `reveal_tasks(...)`：把任务补进 ongoing task 池；
- `set_task_assignment(...)`：记录当前 agent-task 绑定；
- `check_finished_tasks(...)`：检查 agent 是否抵达任务下一站；
- `update_tasks(...)`：每 tick 统一做“更新分配 -> 检查完成 -> 补任务”。

关键点：

- 当前实现**不是固定时刻发任务**，而是**把 ongoing task 池补到阈值 `num_tasks_reveal`**；
- 新任务来源是 `task_id % tasks.size()`，即从任务模板中循环取。

### 3.2 规划流

规划流由 `Entry` 统一驱动：

- `scheduler->plan(...)` 先做任务分配；
- `update_goal_locations(...)` 把分配结果转成每个 agent 当前目标；
- `planner->plan(...)` 再输出多步动作序列。

这里的关键数据容器是 `SharedEnvironment`：

- `task_pool`：当前已 reveal 的任务；
- `new_tasks`、`new_freeagents`：本 tick 的增量信息；
- `curr_task_schedule`：当前调度结果；
- `goal_locations`：每个 agent 的当前目标；
- `curr_states` / `system_states`：当前状态；
- `staged_actions`：当前待执行动作队列。

### 3.3 执行流

执行流是：

1. `simulator.process_new_plan(...)`
   - 调 `Executor::process_new_plan(...)`
   - 把 `Plan` 转成 `staged_actions`
   - 更新预测状态 `predicted_states`
2. `simulator.move(...)`
   - 调 `Executor::next_command(...)`
   - 对每个 agent 输出 `GO/STOP`
   - 再结合 delay / staged_actions / action model 推进真实状态

因此：

- `Planner` 决定**未来一段时间怎么走**；
- `Executor` 决定**当前这一 tick 能不能走**；
- `Simulator` 决定**真实世界里实际发生了什么**。

---

## 4. 关键类与职责

### 4.1 `BaseSystem`

文件：`inc/CompetitionSystem.h` / `src/CompetitionSystem.cpp`

职责：**比赛主控器**。

它把以下模块串起来：

- `TaskManager`
- `Simulator`
- `Entry`
- `SharedEnvironment`
- 线程与时间预算控制

你可以把它理解成：

- 上层调度器：决定什么时候规划；
- 下层驱动器：决定什么时候执行一步；
- 中间胶水：决定环境信息何时同步。

### 4.2 `Entry`

文件：`inc/Entry.h` / `src/Entry.cpp`

职责：**选手算法总入口**。

它本身不直接做复杂算法，而是负责把两件事串起来：

- 任务分配 `TaskScheduler`
- 路径规划 `MAPFPlanner`

### 4.3 `TaskManager`

文件：`inc/TaskManager.h` / `src/TaskManager.cpp`

职责：**任务生命周期管理器**。

负责：

- 当前有哪些 ongoing task；
- 哪些 agent 空闲；
- 哪些 task 完成；
- 当前调度结果是否合法；
- 本 tick 新 reveal 了哪些任务。

### 4.4 `Simulator`

文件：`inc/Simulator.h` / `src/Simulator.cpp`

职责：**真实执行世界模拟器**。

负责：

- 调 `Executor` 拿每 tick 的 `GO/STOP` 决策；
- 把 `staged_actions` 变成真实动作；
- 处理 delay；
- 调 action model 做状态转移；
- 记录 planned path / actual path。

### 4.5 `Executor`

文件：`inc/Executor.h` / `src/Executor.cpp`

职责：**执行层控制器**。

它不负责重新规划全局路径，而负责：

- 接收新 planner 结果；
- 把多步 `Plan` 转成 `staged_actions`；
- 基于依赖图、当前状态和延迟，实时决定谁 `GO`、谁 `STOP`。

### 4.6 `TaskScheduler`

文件：`inc/TaskScheduler.h` / `src/TaskScheduler.cpp`

职责：**任务分配包装层**。

内部会把 `Entry::compute(...)` 给到的总预算切一部分给默认 scheduler。

### 4.7 `MAPFPlanner`

文件：`inc/MAPFPlanner.h` / `src/MAPFPlanner.cpp`

职责：**路径规划包装层**。

内部会：

- 根据总预算扣掉已消耗时间；
- 计算默认多步规划的 `min_plan_steps`；
- 调 `DefaultPlanner::plan(...)`。

---

## 5. 最关键的数据结构

### 5.1 `SharedEnvironment`

文件：`inc/SharedEnv.h`

这是 planner / scheduler / executor 共享的核心上下文。最关键字段：

- **地图与规模**
  - `rows`、`cols`
  - `map`
  - `num_of_agents`
- **状态**
  - `curr_states`
  - `system_states`
  - `curr_timestep`
  - `system_timestep`
- **任务信息**
  - `task_pool`
  - `new_tasks`
  - `new_freeagents`
  - `curr_task_schedule`
  - `goal_locations`
- **执行信息**
  - `staged_actions`
- **时间参数**
  - `plan_start_time`
  - `min_planner_communication_time`
  - `action_time`
  - `max_counter`

### 5.2 `Plan`

文件：`inc/Plan.h`

当前很简单：

- `actions`：`std::vector<std::vector<Action>>`

即：

- 外层按 agent；
- 内层按未来多个 planning step。

这个结构很重要，因为说明：

- 框架并不把计划固定成“只输出下一步”；
- 当前 starter-kit 已经是**多步规划接口**。

### 5.3 `Task`

文件：`inc/Tasks.h`

一个任务包含：

- `task_id`
- `locations`：按顺序依次访问的 errands
- `idx_next_loc`：下一个待完成位置
- `t_revealed`
- `t_completed`
- `agent_assigned`

这说明题目里的一个 task 不是单点，而可能是**多站串行任务**。

### 5.4 `TrajLNS`

文件：`default_planner/TrajLNS.h`

这是默认规划器的“大状态容器”，存了：

- `tasks`：每个 agent 当前目标；
- `trajs`：当前 guide paths；
- `flow`：流量统计；
- `heuristics`：目标启发式表；
- `traj_dists`：到当前 guide path 的距离；
- `goal_nodes`：A* 结果；
- `fw_metrics`：Frank-Wolfe / replanning 排序指标；
- `neighbors`：邻接表；
- `mem`：搜索节点内存池。

---

## 6. 关键函数索引

下面按模块列出最应该先读的函数。

### 6.1 入口与系统调度

#### `driver.cpp`

- **`main(...)`**
  - 读取命令行参数与输入文件；
  - 构造 `BaseSystem`；
  - 设置时间限制与 delay；
  - 启动 `simulate(...)`；
  - 最后保存输出 JSON。

#### `CompetitionSystem.cpp`

- **`BaseSystem::initialize()`**
  - 初始化 `SharedEnvironment`；
  - 初始化 planner / executor；
  - 初始 reveal tasks；
  - 初始化 `proposed_schedule`。

- **`BaseSystem::simulate(int simulation_time, int chunk_size)`**
  - 整个系统最关键的主循环；
  - 负责：初始规划、周期规划、tick 推进、任务更新。

- **`BaseSystem::planner_wrapper()`**
  - 实际调用 `Entry::compute(...)` 的线程包装函数。

- **`BaseSystem::sync_shared_env()`**
  - 把 `TaskManager` 和 `Simulator` 的状态同步进 `SharedEnvironment`。

### 6.2 算法总入口

#### `Entry.cpp`

- **`Entry::initialize(int preprocess_time_limit)`**
  - 离线预处理入口；
  - 分别调用 scheduler 与 planner 的初始化。

- **`Entry::compute(int time_limit, Plan&, vector<int>&)`**
  - 在线规划总入口；
  - 顺序是：
    1. `scheduler->plan(...)`
    2. `update_goal_locations(...)`
    3. `planner->plan(...)`

- **`Entry::update_goal_locations(...)`**
  - 把 `proposed_schedule` 映射成每个 agent 当前要追的 task 下一个 waypoint。

### 6.3 任务管理

#### `TaskManager.cpp`

- **`validate_task_assignment(...)`**
  - 检查调度是否合法；
  - 包括：
    - 重复分配同一任务；
    - 已完成任务被再次分配；
    - 被其他 agent 打开的任务被错误抢占等。

- **`set_task_assignment(...)`**
  - 落地新的 assignment；
  - 更新 `current_assignment`、`agent_assigned`、调度日志。

- **`check_finished_tasks(...)`**
  - 检查 agent 是否到达任务下一站；
  - 如果一个 task 全部 errands 已完成，则释放该 agent。

- **`reveal_tasks(...)`**
  - 把 ongoing task 池补到 `num_tasks_reveal`；
  - 是比赛任务流的关键点。

- **`update_tasks(...)`**
  - 每 tick 的任务入口；
  - 顺序是：更新 assignment -> 检查完成 -> reveal 新任务。

### 6.4 执行层

#### `Simulator.cpp`

- **`Simulator::initialise_executor(...)`**
  - 初始化 executor。

- **`Simulator::process_new_plan(...)`**
  - 接入一版新 planner 输出；
  - 内部会调用 `Executor::process_new_plan(...)`。

- **`Simulator::move(int move_time_limit)`**
  - 每个 execution tick 最关键函数；
  - 内部会：
    1. 调 `executor->next_command(...)`
    2. 结合 staged action 与 delay 生成实际动作
    3. 调 action model 推进一步
    4. 更新 staged action 队列

- **`Simulator::sync_shared_env(...)`**
  - 把执行状态回写到 `SharedEnvironment`。

#### `Executor.cpp`

- **`Executor::initialize(...)`**
  - 初始化依赖图 `tpg`、窗口参数、预测状态辅助结构。

- **`Executor::process_new_plan(...)`**
  - 将 `Plan.actions` append 到 `staged_actions`；
  - 同时基于计划滚动更新 `predicted_states`；
  - 还会维护位置依赖图 `tpg`。

- **`Executor::next_command(...)`**
  - 每 tick 输出每个 agent 是 `GO` 还是 `STOP`；
  - 这是执行控制的核心函数。

- **`Executor::mcp(...)`**
  - 递归判断局部可通行性；
  - 基于 `tpg` 决定当前 agent 是否可以进入目标位置。

### 6.5 默认任务分配器

#### `default_planner/scheduler.cpp`

- **`schedule_initialize(...)`**
  - 初始化启发式与随机种子。

- **`schedule_plan(...)`**
  - 默认调度主函数；
  - 维护 `free_agents` 和 `free_tasks`；
  - 对每个空闲 agent，在所有空闲任务中找估计 makespan 最小的任务。

特点：

- 贪心；
- 依赖静态启发式距离；
- 没有显式建模未来拥堵。

### 6.6 默认多步规划器

#### `src/MAPFPlanner.cpp`

- **`MAPFPlanner::initialize(...)`**
  - 扣除调度已耗时间后，把剩余预算交给默认 planner。

- **`MAPFPlanner::plan(...)`**
  - 计算 `min_plan_steps`；
  - 调 `DefaultPlanner::plan(...)` 输出多步动作。

#### `default_planner/planner.cpp`

- **`DefaultPlanner::initialize(...)`**
  - 初始化优先级、占用表、状态数组、`TrajLNS`、启发式容器。

- **`DefaultPlanner::plan(...)`**
  - 默认规划器的主入口；
  - 分为三段：
    1. 一次性 setup
    2. guide path / flow 优化
    3. 多步 PIBT rollout

- **`initialize_dummy_goals_if_needed(...)`**
  - 给没有任务的 agent 设 dummy goal。

- **`setup_multistep_episode_state(...)`**
  - 准备本轮 planning episode 的临时状态；
  - 设置目标、优先级、`prev_states` / `next_states` 等。

- **`update_guide_paths_once_for_multistep(...)`**
  - 对目标变化的 agent 更新轨迹；
  - 之后做一次 `frank_wolfe(...)`。

- **`refresh_multistep_step_state(...)`**
  - 在 multi-step 规划的 step > 0 时刷新状态；
  - 不重新做完整 guide-path 优化。

- **`run_multistep_pibt_once(...)`**
  - 对当前一个 planning step 运行 PIBT 冲突消解；
  - 生成这一 step 的动作。

- **`append_actions_and_rollout_states(...)`**
  - 把本 step 动作写入输出；
  - 同时在 planner 内部做一次虚拟 rollout，推进 `env->curr_states`。

### 6.7 PIBT 与局部冲突求解

#### `default_planner/pibt.cpp`

- **`get_gp_h(...)`**
  - 获取 agent 到目标或到 guide path 的启发式值。

- **`causalPIBT(...)`**
  - 当前默认局部冲突求解核心；
  - 给 agent 枚举候选邻居和等待动作；
  - 结合优先级与递归让路，决定下一位置。

- **`getAction(...)`**
  - 根据前后位置 / 朝向差异转成 `FW / CR / CCR / W`。

- **`moveCheck(...)`**
  - 在生成 `FW` 之后继续递归检查是否存在链式阻塞；
  - 如果下游不通，则把当前动作回退成 `W`。

### 6.8 启发式、搜索与流量优化

#### `default_planner/heuristics.cpp`

- **`init_neighbor(...)`**
  - 初始化网格邻接表。

- **`init_heuristics(...)`**
  - 初始化全局启发式容器。

- **`init_heuristic(...)`**
  - 为一个 goal 初始化反向 BFS 表。

- **`get_heuristic(...)`**
  - 查询或继续扩展该启发式表。

- **`get_h(...)`**
  - 获取 source 到 target 的距离估计；
  - 默认 scheduler 大量依赖它。

- **`init_dist_2_path(...)` / `get_source_2_path(...)` / `get_dist_2_path(...)`**
  - 维护“当前位置到当前 guide path 的距离”信息；
  - flow / deviation / guide path 相关逻辑会用到。

#### `default_planner/search.cpp`

- **`astar(...)`**
  - 默认的单 agent 路径搜索；
  - 目标不是单纯最短路，而是**在现有 flow 上尽量减少对向流冲突**。

#### `default_planner/flow.cpp`

- **`remove_traj(...)` / `add_traj(...)`**
  - 从 flow 统计里移除 / 加入某个 agent 的 guide path。

- **`update_fw_metrics(...)`**
  - 计算 deviation 等指标，为重规划排序。

- **`frank_wolfe(...)`**
  - 在时限内循环选择 agent 重新规划其 traj；
  - 本质上是一个有限时长的 flow 优化过程。

- **`update_traj(...)`**
  - 用 `astar(...)` 重新为某个 agent 求 traj，并更新相关统计。

---

## 7. 默认算法到底在做什么

一句话概括：

**默认方案 = 贪心任务分配 + 基于 flow 的 guide-path 规划 + 多步 PIBT 实时动作生成 + Executor 负责最终执行放行。**

再展开一点：

1. **任务分配**
   - 给空闲 agent 找一个估计 makespan 最小的 task。

2. **guide path 更新**
   - 对目标变化或轨迹失效的 agent，重新算 traj。

3. **flow 优化**
   - 用 `frank_wolfe(...)` 在预算内不断尝试重规划，降低整体对向冲突。

4. **多步 rollout**
   - 在同一轮 planner 调用中，连续产出多个 step 的动作；
   - 每一步通过 PIBT 做局部冲突消解。

5. **执行层兜底**
   - 即使 planner 给了计划，真正每个 tick 能不能动，还要过 `Executor::next_command(...)`。

---

## 8. 最值得注意的几个实现事实

### 8.1 `Entry::compute(...)` 注释和真实行为有偏差

源码注释写的是“每个 timestep 调一次”，但真实系统不是这样。

真实行为是：

- 第一次启动后调用一次；
- 之后只有在：
  - 上一轮 planner 已结束；
  - 且 `planCommTimeLimit` 窗口到点；
- 才启动下一轮 `compute(...)`。

所以这套系统是**慢环规划 + 快环执行**，不是每 tick 全局重规划。

### 8.2 任务补充是“补库存”而不是“定时发放”

当前任务补充逻辑是：

- 每个 tick 都会调用 `TaskManager::update_tasks(...)`；
- 然后 `reveal_tasks(...)` 会把 ongoing task 池补到 `num_tasks_reveal`。

因此它更像**持续在线任务流**。

### 8.3 `Plan` 是多步计划，不是单步计划

`Plan.actions` 是二维动作数组，所以当前框架天然支持：

- 固定 horizon；
- 可变 horizon；
- 分 agent 多步输出。

### 8.4 `Executor` 不是可有可无的薄封装

这点很重要：

- planner 决定的是“理想动作序列”；
- executor 决定的是“现实执行时本 tick 放不放行”；
- delay 存在时，这一层可能直接影响吞吐和死锁表现。

---

## 9. 如果你想改算法，优先从哪里下手

### 9.1 想提升任务完成数，优先看这三层

- **调度层**：`default_planner/scheduler.cpp`
  - 现在只是简单贪心；
  - 可以考虑拥堵、区域负载、任务链长度、回程成本。

- **规划层**：`default_planner/planner.cpp`
  - 可以改多步 horizon、priority 更新、guide-path 更新频率；
  - 可以增强 rollout 的一致性与鲁棒性。

- **执行层**：`src/Executor.cpp`
  - 可以改 `next_command(...)` 和 `mcp(...)`；
  - 这是 delay 下提升稳定性和防堵塞的直接抓手。

### 9.2 想快速理解 planner，推荐阅读顺序

建议按这个顺序读：

1. `src/Entry.cpp`
2. `src/MAPFPlanner.cpp`
3. `default_planner/planner.cpp`
4. `default_planner/pibt.cpp`
5. `default_planner/flow.cpp`
6. `default_planner/search.cpp`
7. `default_planner/heuristics.cpp`

### 9.3 想快速理解执行层，推荐阅读顺序

1. `src/CompetitionSystem.cpp`
2. `src/Simulator.cpp`
3. `src/Executor.cpp`
4. `inc/SharedEnv.h`

### 9.4 想定位“为什么完成数上不去”

优先排查：

- 调度是否频繁给 agent 分到远距离或拥堵任务；
- planner 的多步动作是否过短或质量不稳；
- executor 是否过于保守导致大量 `STOP`；
- 任务完成后是否及时重新分配；
- delay 下是否出现长链阻塞。

---

## 10. 一页式总结

这套 starter-kit 的系统骨架可以概括成：

- **`driver.cpp`**：读配置并启动系统；
- **`BaseSystem`**：主循环总控，决定何时规划、何时执行、何时更新任务；
- **`Entry`**：算法总入口，先调度再规划；
- **`TaskManager`**：维护在线任务池和任务生命周期；
- **`Simulator` + `Executor`**：负责真实执行和实时放行；
- **`default_planner`**：当前默认算法实现，核心是贪心任务分配 + guide-path / flow 优化 + 多步 PIBT。

如果只记一条主链，可以记成：

**`driver -> BaseSystem::simulate -> Entry::compute -> TaskScheduler / MAPFPlanner -> Simulator / Executor -> TaskManager::update_tasks`**

如果只记一条算法主线，可以记成：

**任务分配 -> 设置当前目标 -> 更新 guide path -> flow 优化 -> 多步 PIBT -> Executor 实时放行 -> tick 执行 -> 任务完成与补充**

---

## 11. 建议你后续继续补充的内容

如果后面你准备深入改默认算法，可以继续在这份文档后面补三类内容：

- **性能瓶颈记录**：哪些函数最耗时；
- **实验日志模板**：每次改动 scheduler / planner / executor 后记录什么指标；
- **赛题特定策略**：比如区域分流、入口控制、任务预留、拥堵预测。

这样 `alg.md` 就会从“代码导读”慢慢变成你的个人算法设计文档。
