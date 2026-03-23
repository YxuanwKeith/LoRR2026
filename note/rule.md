# 项目开发规则

## 规则一：保证受保护文件被覆盖后代码仍能正常运行

评测系统在评测时会用 start-kit 原始版本**覆盖**以下受保护文件，因此我们的改动**绝不能依赖于对这些文件的修改**。

### 任何赛道都不可修改的文件

**src/**
- `ActionModel.cpp`, `Evaluation.cpp`, `Logger.cpp`, `States.cpp`, `driver.cpp`
- `CompetitionSystem.cpp`, `Grid.cpp`, `common.cpp`, `TaskManager.cpp`, `Simulator.cpp`, `DelayGenerator.cpp`

**inc/**
- `ActionModel.h`, `Evaluation.h`, `Logger.h`, `SharedEnv.h`, `Tasks.h`
- `CompetitionSystem.h`, `Grid.h`, `States.h`, `common.h`, `TaskManager.h`, `Simulator.h`, `DelayGenerator.h`

**default_planner/**（全部文件）
- `Memory.h`, `heap.h`, `pibt.cpp`, `search_node.h`, `planner.h`, `search.cpp`, `utils.cpp`
- `TrajLNS.h`, `flow.cpp`, `heuristics.cpp`, `pibt.h`, `scheduler.cpp`, `search.h`, `utils.h`
- `Types.h`, `flow.h`, `heuristics.h`, `planner.cpp`, `scheduler.h`

### Executor 赛道额外不可修改
- `inc/MAPFPlanner.h`, `src/MAPFPlanner.cpp`
- `inc/TaskScheduler.h`, `src/TaskScheduler.cpp`
- `inc/Entry.h`, `src/Entry.cpp`, `inc/Plan.h`

### Scheduler 赛道额外不可修改
- `inc/MAPFPlanner.h`, `src/MAPFPlanner.cpp`
- `inc/Entry.h`, `src/Entry.cpp`
- `inc/Executor.h`, `src/Executor.cpp`, `inc/Plan.h`

### 实践要求

1. **新增的功能代码**统一放在 `optsvr/` 文件夹中（如 `optsvr/MyScheduler.cpp`、`optsvr/MyScheduler.h`），**不要直接改受保护文件**，也不要在 `src/`、`inc/` 下新增文件。
2. **可修改文件**（各赛道对应的实现文件）中的代码，**不能假设受保护文件有任何非原始版本的行为**。
3. `optsvr/` 中的新文件需在 `CMakeLists.txt` 中正确引入（`CMakeLists.txt` 可修改）。
4. 提交前必须用 `LoRR2026_Origin` 中的受保护文件替换本地版本，验证编译和运行正常。

---

## 规则二：不同赛道的代码不能有逻辑强依赖

本项目需要同时提交到多个赛道（Scheduler / Executor / Combined），**同一份代码必须能在不同赛道下正确编译和运行**。

### 赛道与可修改文件对应关系

| 赛道 | 可修改的文件 |
|------|-------------|
| **Scheduler** | `inc/TaskScheduler.h`, `src/TaskScheduler.cpp` |
| **Executor** | `inc/Executor.h`, `src/Executor.cpp` |
| **Combined** | `inc/Entry.h`, `src/Entry.cpp`, `inc/MAPFPlanner.h`, `src/MAPFPlanner.cpp`, `inc/TaskScheduler.h`, `src/TaskScheduler.cpp`, `inc/Executor.h`, `src/Executor.cpp`, `inc/Plan.h` |

### 实践要求

1. **使用 flag 变量区分赛道逻辑**，而非通过条件编译宏硬分叉。例如在可修改文件中：
   ```cpp
   // 在运行时或编译时通过 flag 选择行为
   enum class Track { SCHEDULER, EXECUTOR, COMBINED };
   
   // 示例：根据赛道决定是否启用自定义 Executor 逻辑
   if (current_track == Track::COMBINED) {
       // combined 赛道特有逻辑
   } else {
       // 使用默认行为
   }
   ```

2. **Scheduler 的代码不能依赖自定义 Executor 的存在**（因为 Scheduler 赛道会用默认 Executor）。反之亦然。

3. **共享的工具代码**放在 `optsvr/` 文件夹中（如 `optsvr/MyUtils.cpp`、`optsvr/MyUtils.h`），让不同赛道的实现文件都能引用，但不产生对特定赛道的依赖。

4. 任何赛道下，代码都应该能以如下方式通过编译和测试：
   ```bash
   # 编译
   ./compile.sh
   # 运行（以 warehouse_small 为例）
   ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000
   ```

---

## 规则三：代码清晰可维护，避免复杂调用链

代码质量直接影响后续迭代速度和 debug 效率。**宁可多写几行直白的代码，也不要搞精巧但难懂的调用链**。

### 实践要求

1. **函数职责单一**：每个函数只做一件事，函数名准确描述其行为。避免一个函数内包含多层嵌套或多种不相关逻辑。

2. **调用层级不超过 3 层**：从入口函数到实际执行逻辑，中间的调用跳转不要超过 3 层。如果发现调用链过长，应内联或重组。
   ```
   ❌ 不好：Entry::compute() → StrategyManager::dispatch() → PolicySelector::choose() → WeightCalculator::eval() → ...
   ✅ 好：  Entry::compute() → MyScheduler::schedule() → 直接执行调度逻辑
   ```

3. **避免过度抽象**：不需要为只用一次的逻辑创建接口/基类/策略模式。**先写直白代码，确认需要复用时再抽象**。

4. **注释关键决策**：对于算法选择、参数取值、特殊处理等关键决策，在代码旁写明原因（"为什么"而非"做了什么"）。

5. **新增文件保持小而聚焦**：每个新文件围绕一个明确功能，文件名直接反映用途，统一放在 `optsvr/` 文件夹中。避免出现 `Utils.cpp` 这种万能工具箱越长越大。

---

## 规则四：参数配置采用"编译期默认 + 环境变量运行时覆盖"双层机制

参数控制分两层：
- **`optsvr/Config.h`**：`constexpr` 编译期默认值，提交评测时使用
- **`optsvr/RuntimeConfig.h`**：运行时从环境变量加载，不传环境变量则自动回退到编译期默认值

这样**调参不需要重新编译**，而提交评测时由于不设环境变量，自动使用 `Config.h` 中的默认值。

### 实践要求

1. **所有参数的默认值集中在 `optsvr/Config.h`**：
   ```cpp
   // optsvr/Config.h — 编译期默认参数
   #pragma once
   namespace optsvr {
   constexpr bool  USE_OPT_SCHEDULER   = true;
   constexpr int   HUNGARIAN_THRESHOLD = 200;
   constexpr float DIFFICULTY_WEIGHT   = 0.3f;
   // ... 其他参数
   } // namespace optsvr
   ```

2. **运行时配置通过 `optsvr/RuntimeConfig.h` 的单例访问**：
   ```cpp
   #include "RuntimeConfig.h"
   
   // 在 initialize() 阶段调用一次加载
   optsvr::cfg().load_from_env();
   
   // 使用参数（运行时读取，支持环境变量覆盖）
   if (optsvr::cfg().use_opt_scheduler) {
       // 优化调度逻辑
   } else {
       // baseline 逻辑
   }
   
   float w = optsvr::cfg().difficulty_weight;
   ```

3. **调参方式（不需要重新编译）**：
   ```bash
   # 直接运行（使用编译期默认值）
   ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000
   
   # 通过环境变量覆盖参数
   OPT_DIFFICULTY_WEIGHT=0.5 OPT_HUNGARIAN_THRESHOLD=300 \
     ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000
   
   # 关闭优化调度器跑 baseline
   OPT_USE_OPT_SCHEDULER=0 \
     ./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000
   ```

4. **环境变量命名约定**：统一 `OPT_` 前缀 + 大写参数名
   | 环境变量 | 对应 Config.h 常量 | 类型 |
   |---------|-------------------|------|
   | `OPT_USE_OPT_SCHEDULER` | `USE_OPT_SCHEDULER` | bool (0/1/true/false) |
   | `OPT_HUNGARIAN_THRESHOLD` | `HUNGARIAN_THRESHOLD` | int |
   | `OPT_CANDIDATE_TOPK` | `CANDIDATE_TOPK` | int |
   | `OPT_CONGESTION_DECAY` | `CONGESTION_DECAY` | float |
   | `OPT_CONGESTION_TTL` | `CONGESTION_TTL` | int |
   | `OPT_DIFFICULTY_WEIGHT` | `DIFFICULTY_WEIGHT` | float |
   | `OPT_REGION_SIZE` | `REGION_SIZE` | int |

5. **不使用以下方式传参**：
   - ❌ JSON 配置文件（`config.json`）
   - ❌ `RuntimeOptions` 键值对系统
   - ❌ 在 `driver.cpp` 中注册 `boost::program_options`（受保护文件）
   - ❌ 修改 `SharedEnv.h` 添加自定义字段（受保护文件）

6. **提交安全保证**：提交评测时不设置任何 `OPT_*` 环境变量，`RuntimeConfig` 自动使用 `Config.h` 中的编译期默认值，**等同于 `constexpr` 方案的效果，零额外开销**。

---

## 规则五：任务分配和路径规划必须有效率保证，严防超时

评测系统对每次 `compute` 调用（包含任务分配 + 路径规划）有严格的时间限制。**一旦 plan 模块超时，系统会使用上一轮的旧路径继续执行，导致后续多轮规划质量严重下降，形成恶性循环**。因此所有新增模块必须把效率作为硬约束。

### 超时机制说明

| 阶段 | 时间限制 | 说明 |
|------|---------|------|
| 预处理 `initialize` | `preprocessTimeLimit`（默认 30s） | 离线阶段，仅执行一次 |
| 每轮规划 `compute` | `planCommTimeLimit`（默认 1s） | Scheduler + Planner 共享此预算 |
| Scheduler 分配 | 约 `time_limit/2` | 默认占一半时间 |
| Planner 路径规划 | 剩余时间 - 容差 | 扣除 Scheduler 耗时和 `PLANNER_TIMELIMIT_TOLERANCE` |

### 实践要求

1. **Scheduler（任务分配）耗时必须可控**：
   - 大规模场景（如 5000 agent）**禁止**使用 O(n³) 的全局匈牙利算法，应使用贪心/局部匹配等线性或近线性方案
   - 必须设置内部 deadline，在接近 `time_limit/2` 时立即返回当前最优结果，不能等到外部超时
   - 每轮只处理空闲 agent 的分配，不要对已分配的 agent 重新全局计算

2. **Planner（路径规划）耗时必须可控**：
   - 路径规划的可用时间 = `time_limit` - Scheduler 实际耗时 - 容差，必须在此预算内完成
   - 使用 anytime 算法风格：先快速得到可行解，有剩余时间再优化
   - 避免在单次规划中引入高复杂度的全局搜索

3. **新模块必须做耗时监控**：
   - 新增的任务分配或路径规划逻辑中，必须在关键循环中检查 `std::chrono::steady_clock`，及时跳出
   - 示例模式：
     ```cpp
     auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_budget);
     for (auto& agent : free_agents) {
         if (std::chrono::steady_clock::now() >= deadline) break;  // 超时保护
         // ... 分配/规划逻辑 ...
     }
     ```

4. **禁止无 deadline 保护的循环**：任何可能随 agent/task 规模增长的循环，都必须有 deadline 检查或提前确认复杂度在 O(n log n) 以内。

5. **性能基准**：新模块在 `warehouse_small_200`（200 agent）场景下，单轮 `compute` 耗时不应超过 500ms，留足余量给其他模块。

---

## 规则六：充分考虑数据传输的时序问题

系统中 `SharedEnvironment`（`env`）是 Scheduler、Planner、Executor 之间的唯一数据桥梁。**`env` 中的字段在不同阶段有不同的值，读写顺序错误会导致使用过期数据或空数据**。

### 系统数据更新时序

```
┌─ CompetitionSystem 主循环 ─────────────────────────────────────┐
│                                                                 │
│  1. simulator.move()         → 执行动作，agent 位置更新         │
│  2. task_manager.update_tasks() → 完成的任务标记，新 free agent  │
│  3. task_manager.reveal_tasks() → 新任务加入 task_pool           │
│  4. sync_shared_env()        → 同步到 env（curr_states, new_tasks, │
│                                 new_freeagents, curr_timestep）  │
│  5. env->plan_start_time = now()                                │
│  6. Entry::compute(time_limit, plan, schedule)                  │
│     ├─ scheduler->plan()     → 读 env->new_freeagents/new_tasks │
│     ├─ update_goal_locations() → 写 env->goal_locations         │
│     └─ planner->plan()      → 读 env->goal_locations/curr_states│
│  7. task_manager.clear_new_agents_tasks() → 清空 new_* 字段     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 关键时序约束

| 字段 | 写入时机 | 有效读取时机 | 常见错误 |
|------|---------|-------------|---------|
| `env->new_freeagents` | `sync_shared_env()` 后填充 | `scheduler->plan()` 内 | 在 planner 中读取（已被 clear） |
| `env->new_tasks` | `reveal_tasks()` + `sync` | `scheduler->plan()` 内 | 在 initialize 阶段读取（尚未填充） |
| `env->curr_states` | `sync_shared_env()` 时同步 | `compute()` 全程 | 缓存旧值后未刷新 |
| `env->goal_locations` | `update_goal_locations()` 写入 | `planner->plan()` 内 | 在 scheduler 中读取（还是上一轮的值） |
| `env->curr_task_schedule` | `update_goal_locations()` 写入 | `planner->plan()` 内 | 在 scheduler 中读取（还是上一轮的值） |
| `env->task_pool` | 系统持续更新 | 任何时候（只读） | 假设 task_id 连续或从 0 开始 |
| `env->plan_start_time` | 每次调 `compute` 前由系统设置 | `compute()` 全程 | 自己覆盖这个值 |

### 实践要求

1. **Scheduler 只读当轮新增数据**：`env->new_freeagents` 和 `env->new_tasks` 仅在当轮 `compute` 调用期间有效，**不要缓存或跨轮使用**。如果需要维护历史自由 agent 集合，必须在自己的模块中单独维护（参考 `default_planner/scheduler.cpp` 中的 `free_agents` / `free_tasks`）。

2. **Planner 不要读取 `new_freeagents` / `new_tasks`**：这些字段在 `scheduler->plan()` 执行后可能已被系统清空，Planner 应通过 `env->goal_locations` 和 `env->curr_task_schedule` 获取调度结果。

3. **不要在自定义代码中写入 `env` 的系统管理字段**：以下字段由 `CompetitionSystem` 管理，自定义代码**只读不写**：
   - `curr_states`、`curr_timestep`、`system_timestep`
   - `new_freeagents`、`new_tasks`
   - `task_pool`
   - `plan_start_time`、`map`、`num_of_agents`

4. **可写字段仅限**：
   - `goal_locations`：在 `update_goal_locations()` 中写入（Entry.cpp 负责）
   - `curr_task_schedule`：在 `update_goal_locations()` 中写入
   - `staged_actions`：Executor 在 `plan()` 中写入

5. **自维护状态必须处理首轮特殊情况**：第一轮 `compute` 时，所有 agent 都在 `new_freeagents` 中，`curr_task_schedule` 全为 -1。自维护的数据结构需要正确处理这个初始状态。

6. **跨模块数据传递走 `env` 的约定字段**：Scheduler 和 Planner 之间的数据传递**必须通过 `env->curr_task_schedule` 和 `env->goal_locations`**（由 `update_goal_locations()` 桥接），不要自行创建其他共享数据通道。

---

## 规则七：数据结构传输效率

在函数间传递 `vector` 等容器时，**必须使用引用（`const &` 或 `&`）**，禁止不必要的深拷贝。

### 实践要求

1. **只读参数用 `const &`**：
   ```cpp
   // ✅ 好
   MatchResult match(const std::vector<AgentInfo>& agents,
                     const std::vector<TaskInfo>& tasks);
   
   // ❌ 坏：会深拷贝整个 vector
   MatchResult match(std::vector<AgentInfo> agents,
                     std::vector<TaskInfo> tasks);
   ```

2. **需要修改的参数用 `&`**：
   ```cpp
   void apply_matches(const MatchResult& matches,
                      std::vector<int>& proposed_schedule);
   ```

3. **返回值可用值语义**（编译器 RVO/NRVO 会消除拷贝）：
   ```cpp
   // ✅ 好：RVO 会优化，不会实际拷贝
   std::vector<int> get_visible_tasks(int grid_id) const;
   ```

4. **内部临时变量用 `reserve` 预分配**，减少动态扩容开销。

5. **禁止在循环中反复构造/销毁大容器**，应在外层声明后 `clear()` 复用。

---

## 检查清单（提交前必查）

- [ ] 受保护文件没有被修改（或已还原为原始版本）
- [ ] `CMakeLists.txt` 中正确包含了 `optsvr/` 下所有新增源文件
- [ ] 在 Scheduler 赛道配置下编译运行正常
- [ ] 在 Executor 赛道配置下编译运行正常
- [ ] 在 Combined 赛道配置下编译运行正常
- [ ] `optsvr/` 中的新增文件不依赖对受保护文件的任何修改
- [ ] 不同赛道的自定义逻辑之间无强耦合
- [ ] 新增代码函数职责单一，调用链不超过 3 层
- [ ] 没有不必要的过度抽象（接口/基类/策略模式只在确需复用时使用）
- [ ] 每个新增参数在 `optsvr/Config.h` 有编译期默认值，在 `RuntimeConfig.h` 有环境变量读取
- [ ] 新模块的主开关默认值使 baseline 行为完全不变（`USE_OPT_SCHEDULER = false` 时零开销）
- [ ] 参数读取统一通过 `optsvr::cfg()` 访问，不直接使用 `getenv()` 散落在各处
- [ ] 环境变量命名遵循 `OPT_` 前缀约定
- [ ] 任务分配和路径规划模块的关键循环都有 deadline 检查
- [ ] 新模块在 warehouse_small_200 场景下单轮 compute 不超过 500ms
- [ ] 自定义代码未写入 env 的系统管理字段（curr_states、new_freeagents 等）
- [ ] Scheduler 未跨轮缓存 new_freeagents / new_tasks（每轮从 env 重新读取或自维护）
- [ ] Planner 通过 goal_locations / curr_task_schedule 获取调度结果，未直接读 new_* 字段
- [ ] 首轮（所有 agent 在 new_freeagents 中、schedule 全为 -1）运行正常
