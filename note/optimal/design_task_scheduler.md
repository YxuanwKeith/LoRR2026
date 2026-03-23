# 任务分配优化 — 框架设计

## 一、整体思路

将任务分配从默认的「贪心最短距离」升级为「打分 + 匹配」两阶段方案。

**开关控制方案**：使用 `optsvr/Config.h` 中的 **`constexpr` 编译期常量** 控制所有开关和参数。不使用环境变量、不使用配置文件、不修改受保护文件。切换参数只需改一个头文件然后重新编译。

```
TaskScheduler::plan()
    │
    ├─ [optsvr::USE_OPT_SCHEDULER == false] → DefaultPlanner::schedule_plan()  （原逻辑）
    │
    └─ [optsvr::USE_OPT_SCHEDULER == true]  → optsvr::OptScheduler::schedule()
                                        │
                                        ├─ 1) 收集空闲 agent 和空闲 task
                                        ├─ 2) 打分模块：计算每个 (agent, task) 的代价
                                        │       ├─ 适配度：agent 当前位置到 task 起点的距离
                                        │       └─ 任务难度：任务长度 + 起点区域拥塞度
                                        ├─ 3) 匹配模块：根据打分结果分配
                                        │       ├─ 小规模（≤ 阈值）：匈牙利匹配
                                        │       └─ 大规模（> 阈值）：区域筛选 + 局部匹配
                                        └─ 4) 输出 proposed_schedule
```

---

## 二、新增文件（全部在 `optsvr/`）

| 文件 | 职责 | 说明 |
|------|------|------|
| `optsvr/Config.h` | 参数控制中心 | 所有开关和参数的 `constexpr` 编译期常量 |
| `optsvr/OptScheduler.h` | 优化调度器头文件 | 声明 `OptScheduler` 类 |
| `optsvr/OptScheduler.cpp` | 优化调度器实现 | 主调度流程：打分 → 匹配 → 输出 |
| `optsvr/TaskScorer.h` | 任务打分模块头文件 | 声明打分接口 |
| `optsvr/TaskScorer.cpp` | 任务打分模块实现 | 适配度 + 任务难度 + 拥塞度计算 |
| `optsvr/CongestionMap.h` | 拥塞热力图头文件 | 声明区域拥塞度维护接口 |
| `optsvr/CongestionMap.cpp` | 拥塞热力图实现 | 基于 agent 路径衰减更新拥塞度 |
| `optsvr/HungarianMatcher.h` | 匈牙利匹配头文件 | 声明小规模匹配接口 |
| `optsvr/HungarianMatcher.cpp` | 匈牙利匹配实现 | 标准匈牙利算法 |
| `optsvr/RegionMatcher.h` | 区域匹配头文件 | 声明大规模区域筛选 + 局部匹配接口 |
| `optsvr/RegionMatcher.cpp` | 区域匹配实现 | 地图分区、候选筛选、局部匹配 |

---

## 三、修改的已有文件

### 1. `optsvr/Config.h`（新增 — 唯一的参数控制中心）

所有开关和参数集中在这一个头文件中，用 `constexpr` 编译期常量定义。切换时只需改此文件并重新编译。

```cpp
// optsvr/Config.h
#pragma once

namespace optsvr {

// ─── 主开关 ───
constexpr bool USE_OPT_SCHEDULER = true;   // false = 等同 baseline

// ─── 匹配策略 ───
constexpr int  HUNGARIAN_THRESHOLD = 200;  // agent 数量 ≤ 此值时用匈牙利，否则用区域匹配
constexpr int  CANDIDATE_TOPK      = 50;   // 大规模场景下每个 agent 的候选任务数量上限

// ─── 拥塞热力图 ───
constexpr int  REGION_SIZE         = 4;    // 区域划分粒度（region_size × region_size）
constexpr float CONGESTION_DECAY   = 0.9f; // 路径拥塞权重的逐步衰减系数
constexpr int  CONGESTION_TTL      = 100;  // 拥塞信息的有效 tick 数

// ─── 打分权重 ───
constexpr float DIFFICULTY_WEIGHT  = 0.3f; // 任务难度在打分中的权重（vs 适配度）

} // namespace optsvr
```

**为什么用 `constexpr` 而不用环境变量/配置文件：**
- **零运行时开销**：编译期常量，关闭时死代码直接被优化掉，不占一行指令
- **零依赖**：不改 `driver.cpp`、不改 `SharedEnv.h`、不需要 `getenv()`/json 解析
- **一目了然**：所有参数集中在 40 行代码里，grep 一下就知道当前配置
- **提交时一键切回**：把 `USE_OPT_SCHEDULER` 改成 `false` 即可等同 baseline

### 2. `src/TaskScheduler.cpp`（Scheduler 赛道可修改）

```cpp
#include "TaskScheduler.h"
#include "scheduler.h"
#include "const.h"
#include "optsvr/Config.h"
#include "optsvr/OptScheduler.h"

// 持有优化调度器实例（static，生命周期跟随进程）
static optsvr::OptScheduler g_opt_scheduler;

void TaskScheduler::initialize(int preprocess_time_limit)
{
    int limit = preprocess_time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;
    DefaultPlanner::schedule_initialize(limit, env);

    if constexpr (optsvr::USE_OPT_SCHEDULER) {
        g_opt_scheduler.initialize(limit, env);
    }
}

void TaskScheduler::plan(int time_limit, std::vector<int> & proposed_schedule)
{
    int limit = time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;

    if constexpr (optsvr::USE_OPT_SCHEDULER) {
        g_opt_scheduler.schedule(limit, proposed_schedule, env);
    } else {
        DefaultPlanner::schedule_plan(limit, proposed_schedule, env);
    }
}
```

> **注意**：使用 `if constexpr` 而非 `if`，关闭时编译器会完全移除 OptScheduler 相关代码，不产生任何运行时开销。`g_opt_scheduler` 是栈上对象（static 局部），不需要 `new`/`delete`。

### 3. `inc/TaskScheduler.h`（不修改）

头文件接口保持与原始版本完全一致，不需要修改。

### 4. `src/driver.cpp`（受保护文件，不修改）

**完全不需要修改 `driver.cpp`**。所有参数都在 `optsvr/Config.h` 中定义，不需要命令行参数注册。

### 运行方式

```bash
# 编译（Config.h 中 USE_OPT_SCHEDULER = true 时启用）
./compile.sh

# 直接运行，无需任何额外参数
./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000

# 切回 baseline：将 Config.h 中 USE_OPT_SCHEDULER 改为 false，重新编译
```

---

## 四、参数控制设计

所有参数集中在 `optsvr/Config.h` 中，使用 `constexpr` 编译期常量。

| 常量名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `USE_OPT_SCHEDULER` | bool | `true` | 主开关，`false` = 等同 baseline |
| `HUNGARIAN_THRESHOLD` | int | `200` | agent 数量 ≤ 此值时使用匈牙利匹配，否则使用区域匹配 |
| `CANDIDATE_TOPK` | int | `50` | 大规模场景下每个 agent 的候选任务数量上限 |
| `REGION_SIZE` | int | `4` | 拥塞地图的区域划分粒度（region_size × region_size） |
| `CONGESTION_DECAY` | float | `0.9` | 路径拥塞权重的逐步衰减系数 |
| `CONGESTION_TTL` | int | `100` | 拥塞信息的有效 tick 数 |
| `DIFFICULTY_WEIGHT` | float | `0.3` | 打分中任务难度的权重（vs 适配度） |

**切换方式**：修改 `optsvr/Config.h` 中对应常量的值，然后 `./compile.sh` 重新编译即可。

---

## 五、核心类接口设计

### OptScheduler（主调度器）

```cpp
// optsvr/OptScheduler.h
#pragma once
#include "SharedEnv.h"
#include <vector>
#include <unordered_set>

namespace optsvr {

class OptScheduler {
public:
    void initialize(int time_limit, SharedEnvironment* env);
    void schedule(int time_limit, std::vector<int>& proposed_schedule, SharedEnvironment* env);

private:
    // 内部状态（自维护，跨轮保持）
    std::unordered_set<int> free_agents;
    std::unordered_set<int> free_tasks;

    // 打分：计算 agent i 执行 task t 的代价
    float score_agent_task(int agent_id, int task_id, SharedEnvironment* env);

    // 小规模匹配
    void match_hungarian(
        const std::vector<int>& agents,
        const std::vector<int>& tasks,
        const std::vector<std::vector<float>>& cost_matrix,
        std::vector<int>& proposed_schedule);

    // 大规模匹配：筛选候选 + 局部贪心
    void match_region(
        int time_limit,
        const std::vector<int>& agents,
        const std::vector<int>& tasks,
        std::vector<int>& proposed_schedule,
        SharedEnvironment* env);
};

} // namespace optsvr
```

> **注意**：不再有 `SchedulerConfig` 结构体和 `load_config()` 方法。所有参数直接引用 `optsvr/Config.h` 中的 `constexpr` 常量（如 `optsvr::HUNGARIAN_THRESHOLD`），零运行时开销。

### TaskScorer（打分模块）

```cpp
// optsvr/TaskScorer.h
#pragma once
#include "SharedEnv.h"

namespace optsvr {

// 计算 agent 到 task 起点的适配度（距离越近分越低 = 代价越小）
float compute_fitness(int agent_loc, int task_start_loc, SharedEnvironment* env);

// 计算任务固有难度（任务总长度）
float compute_task_difficulty(int task_id, SharedEnvironment* env);

// 计算任务起点区域的拥塞惩罚
float compute_congestion_penalty(int task_start_loc, SharedEnvironment* env);

// 综合打分 = fitness + difficulty_weight * (difficulty + congestion_penalty)
float compute_score(int agent_id, int task_id, float difficulty_weight, SharedEnvironment* env);

} // namespace optsvr
```

### CongestionMap（拥塞热力图）

```cpp
// optsvr/CongestionMap.h
#pragma once
#include "SharedEnv.h"
#include <vector>

namespace optsvr {

class CongestionMap {
public:
    // 初始化：根据地图大小和区域粒度建立区域划分
    void initialize(int rows, int cols, int region_size);

    // 每轮更新：根据当前 agent 位置和规划路径更新拥塞度
    void update(SharedEnvironment* env, float decay);

    // 检查时效性并清理过期数据
    void tick_expire(int current_timestep, int ttl);

    // 查询某个格子所在区域的拥塞度
    float get_congestion(int location) const;

    // 查询某个格子周围 9 宫格区域的平均拥塞度
    float get_neighborhood_congestion(int location) const;

private:
    int rows, cols, region_size;
    int region_rows, region_cols;
    std::vector<float> region_congestion;  // 每个区域的拥塞度

    int loc_to_region(int location) const;
};

// 全局单例（在 OptScheduler::initialize 中初始化）
CongestionMap& get_congestion_map();

} // namespace optsvr
```

### RegionMatcher（大规模区域匹配）

```cpp
// optsvr/RegionMatcher.h
#pragma once
#include "SharedEnv.h"
#include <vector>

namespace optsvr {

// 为每个 agent 筛选 topk 个最近的候选任务
// 返回 candidates[agent_idx] = {task_id_1, task_id_2, ...}
std::vector<std::vector<int>> filter_candidates(
    const std::vector<int>& agents,
    const std::vector<int>& tasks,
    int topk,
    SharedEnvironment* env);

// 基于候选列表做贪心匹配（带 deadline 保护）
void greedy_match(
    int time_limit,
    const std::vector<int>& agents,
    const std::vector<int>& tasks,
    const std::vector<std::vector<int>>& candidates,
    const std::vector<std::vector<float>>& candidate_scores,
    std::vector<int>& proposed_schedule);

} // namespace optsvr
```

---

## 六、CMakeLists.txt 修改

```cmake
# 新增 optsvr 目录
include_directories("optsvr")
file(GLOB OPTSVR_SOURCES "optsvr/*.cpp")

# 修改 SOURCES 定义，加入 optsvr
file(GLOB SOURCES "src/*.cpp" "default_planner/*.cpp" "optsvr/*.cpp")
```

---

## 七、调用流程图

```
每轮 tick:
  CompetitionSystem::simulate()
    → Entry::compute(time_limit, plan, proposed_schedule)
        → TaskScheduler::plan(time_limit, proposed_schedule)
            │
            ├─ [USE_OPT_SCHEDULER == false]  （编译期消除，零开销）
            │   → DefaultPlanner::schedule_plan()
            │       遍历 free_agents，贪心选最短距离任务
            │
            └─ [USE_OPT_SCHEDULER == true]
                → OptScheduler::schedule()
                    1. 收集 free_agents / free_tasks
                    2. CongestionMap::update()  更新拥塞热力图
                    3. CongestionMap::tick_expire()  清理过期数据
                    4. 判断规模:
                       ├─ ≤ HUNGARIAN_THRESHOLD:
                       │   a. 构建 cost_matrix[agent][task] = compute_score(...)
                       │   b. match_hungarian() → proposed_schedule
                       │
                       └─ > HUNGARIAN_THRESHOLD:
                           a. filter_candidates() 每个 agent 选 CANDIDATE_TOPK 候选
                           b. 对候选打分
                           c. greedy_match() → proposed_schedule
                    5. deadline 检查贯穿全程

        → Entry::update_goal_locations(proposed_schedule)
        → MAPFPlanner::plan(time_limit, plan)
```

---

## 八、运行方式

```bash
# 编译（optsvr/Config.h 中 USE_OPT_SCHEDULER = true 时自动启用）
./compile.sh

# 直接运行，无需任何额外参数或环境变量
./build/lifelong -i example_problems/warehouse.domain/warehouse_small_200.json -s 1000

# 如需切回 Baseline：
# 1. 将 optsvr/Config.h 中 USE_OPT_SCHEDULER 改为 false
# 2. 重新编译 ./compile.sh
# 3. 运行（行为与原始 baseline 完全一致，零开销）

# 如需调参：
# 直接修改 optsvr/Config.h 中对应的 constexpr 值，重新编译即可
# 例如将 HUNGARIAN_THRESHOLD 从 200 改为 300，DIFFICULTY_WEIGHT 从 0.3 改为 0.4
```

---

## 九、规则检查

- [x] 新代码全部在 `optsvr/`，不在 `src/`、`inc/` 下新增文件
- [x] 只修改 `src/TaskScheduler.cpp`（Scheduler 赛道可修改文件）
- [x] `inc/TaskScheduler.h` 不修改，接口与原始版本一致
- [x] 受保护文件（`driver.cpp`、`SharedEnv.h` 等）不做任何修改
- [x] 通过 `optsvr/Config.h` 中 `USE_OPT_SCHEDULER = false` 可切回 baseline，编译期消除零开销
- [x] 所有循环有 deadline 检查，大规模有区域筛选降低复杂度
- [x] 调用层级：TaskScheduler → OptScheduler → 打分/匹配，不超过 3 层
- [x] 各赛道无强依赖（Scheduler 赛道只改 TaskScheduler，不影响 Executor）
- [x] 不使用 `getenv()`、不解析 JSON 配置文件、不依赖 RuntimeOptions
