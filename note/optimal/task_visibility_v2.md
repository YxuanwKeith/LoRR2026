# Task Visibility V2 — 自适应区域 + 稀疏二部图匹配

## 1. 整体分层架构

```
OptScheduler::schedule()
  │
  ├─ 传入：agents（free_agents_）、tasks（free_tasks_）
  │
  └─ TaskMatcher::match(agents, tasks, deadline)    ← 封装后的唯一入口
       │
       │  ===== TaskMatcher 内部流程 =====
       │
       ├─ Step 1: 将 tasks 按起点放入 visibility grid
       │    GridPartition + GridTaskIndex
       │
       ├─ Step 2: 每个 agent 根据所在 grid 查 3×3 宫格，
       │          对可见 tasks 打分并建稀疏边
       │    adj[agent_idx] = [(task_global_id, score), ...]
       │
       ├─ Step 3: 在稀疏二部图上跑匹配算法
       │    小规模 → regret / 匈牙利
       │    大规模 → 增广路或 auction
       │
       └─ Step 4: 返回 MatchResult
```

**关键设计**：`TaskMatcher::match()` 的外部接口不变——仍然是传入 `agents` 和 `tasks`，但内部自动完成区域划分、可见范围筛选、稀疏建边和匹配。OptScheduler 调用方无感知。

## 2. GridPartition — 通用地图分区工具

### 2.1 类设计

```cpp
// optsvr/GridPartition.h
class GridPartition {
public:
    // 初始化
    void init(int map_rows, int map_cols, int grid_rows, int grid_cols);

    // cell → grid
    int cell_to_grid(int loc) const;

    // grid → (row, col)
    std::pair<int,int> grid_to_rc(int grid_id) const;

    // 3×3 宫格邻域（含自身，边界裁剪）
    // 返回值：写入 out，返回个数（避免每次 alloc vector）
    int get_neighbors_3x3(int grid_id, int* out) const;  // out 至少 9 格

    // 访问器
    int rows() const;
    int cols() const;
    int count() const;

private:
    int map_rows_, map_cols_;
    int grid_rows_, grid_cols_;
    int cell_h_, cell_w_;  // 每个 grid 的 cell 尺寸
};
```

### 2.2 两个独立实例

- `congestion_grid_`：拥塞用，固定 `cell_size=4`（小格密集）
- `visibility_grid_`：可见范围用，自适应计算（大格稀疏）

## 3. 自适应 Grid 数量

### 3.1 公式

```
目标：每个 agent 可见约 TARGET_VISIBLE_TASKS 个任务

coarse_grids = ceil(total_tasks / TARGET_VISIBLE_TASKS)

if (num_agents < TARGET_VISIBLE_TASKS):
    coarse_grids = 1   // 全局匹配，不分区

// 细分 ×3 → fine grid
fine_rows = coarse_rows * 3
fine_cols = coarse_cols * 3

// 每个 agent 看 3×3 fine grids = 9 个
// 9 fine grids ≈ 1 coarse grid 面积
// 可见任务 ≈ total_tasks / coarse_grids = TARGET_VISIBLE_TASKS ✓
```

### 3.2 数值示例

| 场景 | agents | tasks | coarse | fine grid | 可见 tasks |
|------|--------|-------|--------|-----------|-----------|
| warehouse_large | 5000 | ~7500 | 15 (3×5) | 9×15=135 | ~500 |
| warehouse_small | 200 | ~200 | 1 (1×1) | 3×3=9 | ~200 (全局) |
| game_brc202d | 500 | ~750 | 2 (1×2) | 3×6=18 | ~375 |
| random_32 | 100 | ~150 | 1 (1×1) | 3×3=9 | ~150 (全局) |

注意：agent<500 时 coarse=1，fine=3×3=9，每个 agent 看 3×3=9 格覆盖全部 → 等价全局。

## 4. 稀疏二部图建边

### 4.1 数据结构

```cpp
// 稀疏邻接表（agent 侧）
struct Edge {
    int task_idx;   // task 在 tasks 数组中的下标
    float cost;     // 打分（越低越好）
};

// adj[agent_idx] = 该 agent 的所有可见 task 边
std::vector<std::vector<Edge>> adj;   // 大小 = agents.size()
```

### 4.2 建边流程（TaskMatcher 内部）

```
void build_bipartite_graph(
    const vector<AgentInfo>& agents,
    const vector<TaskInfo>& tasks,
    vector<vector<Edge>>& adj,          // out: 稀疏邻接表
    deadline)
{
    // 1. 构建 task → grid 索引
    //    grid_tasks[grid_id] = [task 在 tasks 数组中的下标, ...]
    vector<vector<int>> grid_tasks(visibility_grid_.count());
    for (int ti = 0; ti < tasks.size(); ti++) {
        int gid = visibility_grid_.cell_to_grid(tasks[ti].start_location);
        grid_tasks[gid].push_back(ti);
    }

    // 2. 每个 agent 对可见 tasks 打分建边
    adj.resize(agents.size());
    for (int ai = 0; ai < agents.size(); ai++) {
        if (now > deadline) break;

        int agent_gid = visibility_grid_.cell_to_grid(agents[ai].location);
        int neighbors[9];
        int n_neighbors = visibility_grid_.get_neighbors_3x3(agent_gid, neighbors);

        adj[ai].clear();
        for (int k = 0; k < n_neighbors; k++) {
            for (int ti : grid_tasks[neighbors[k]]) {
                float s = score(agents[ai], tasks[ti]);
                adj[ai].push_back({ti, s});
            }
        }

        // 如果 adj[ai] 为空（9 宫格无 task），标记需要保底
    }
}
```

### 4.3 保底策略

```
// 3. 保底：可见范围为空的 agent → 对全局 tasks 建边
for (int ai : agents_without_visible_tasks) {
    for (int ti = 0; ti < tasks.size(); ti++) {
        float s = score(agents[ai], tasks[ti]);
        adj[ai].push_back({ti, s});
    }
}
```

### 4.4 边数分析

| 场景 | agents | 可见/agent | 总边数 | 对比全连接 |
|------|--------|-----------|--------|-----------|
| warehouse_large | 5000 | ~500 | 250万 | 3750万 (↓15x) |
| warehouse_small | 200 | ~200 | 4万 | 4万 (相同) |

## 5. 稀疏图上的匹配算法

### 5.1 策略选择

建完稀疏边后，根据规模选择匹配算法：

```cpp
MatchResult sparse_match(
    const vector<AgentInfo>& agents,
    const vector<TaskInfo>& tasks,
    const vector<vector<Edge>>& adj,
    deadline)
{
    int n = agents.size();

    if (n <= cfg().hungarian_threshold) {
        // 小规模：稀疏 regret-based
        return sparse_regret_match(agents, tasks, adj, deadline);
    } else {
        // 大规模：贪心 TopK（已经是稀疏的了）
        return sparse_greedy_match(agents, tasks, adj, deadline);
    }
}
```

### 5.2 sparse_regret_match（小规模精确）

```
已有的 regret 算法，但不再构建 N×M 稠密矩阵。
每个 agent 的候选 tasks 就是 adj[ai] 中的边。

对每个 agent：
  best_score = adj[ai] 中最小 cost 的边
  second_best = 第二小
  regret = second_best - best_score

按 regret 降序排列 agents，依次分配 best task（如已被占，从 adj 中找次优）。
```

### 5.3 sparse_greedy_match（大规模快速）

```
按某种优先级排序 agents（如到最近 task 距离降序）。
对每个 agent：
  从 adj[ai] 中选 cost 最小且未被占用的 task。
```

## 6. 完整 TaskMatcher 接口（修改后）

```cpp
class TaskMatcher {
public:
    void set_env(SharedEnvironment* env);
    void set_congestion_func(std::function<float(int)> func);

    // ============ 唯一核心接口 ============
    // 传入全部 agents 和 tasks，内部自动处理：
    //   区域划分 → 可见范围建边 → 稀疏匹配 → 返回结果
    MatchResult match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        std::chrono::steady_clock::time_point deadline);

    // 打分（供外部调用）
    float score(const AgentInfo& agent, const TaskInfo& task) const;

private:
    SharedEnvironment* env_ = nullptr;
    std::function<float(int)> congestion_func_ = nullptr;

    // ---- 可见范围 grid（与拥塞 grid 独立）----
    GridPartition visibility_grid_;
    bool grid_initialized_ = false;

    // 初始化 visibility grid（首次 match 时按 agents/tasks 规模自适应计算）
    void ensure_grid_initialized(int num_agents, int num_tasks);

    // ---- 内部流程 ----

    // Step 1+2: 建稀疏二部图
    void build_bipartite_graph(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        std::vector<std::vector<Edge>>& adj,
        std::vector<int>& fallback_agents,        // out: 需要保底的 agent 下标
        std::chrono::steady_clock::time_point deadline);

    // Step 3a: 稀疏 regret 匹配（小规模）
    MatchResult sparse_regret_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const std::vector<std::vector<Edge>>& adj,
        std::chrono::steady_clock::time_point deadline);

    // Step 3b: 稀疏贪心匹配（大规模）
    MatchResult sparse_greedy_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const std::vector<std::vector<Edge>>& adj,
        std::chrono::steady_clock::time_point deadline);
};
```

## 7. OptScheduler 调用方式（极简化）

```cpp
void OptScheduler::schedule(int time_limit,
                            std::vector<int>& proposed_schedule,
                            SharedEnvironment* env) {
    auto deadline = now() + ms(time_limit);

    // 1. 收集 free agents/tasks
    free_agents_.insert(env->new_freeagents...);
    free_tasks_.insert(env->new_tasks...);

    // 2. 更新拥塞（用 congestion_grid_，与 visibility 无关）
    update_congestion(env);

    if (free_agents_.empty() || free_tasks_.empty()) {
        for (int a : free_agents_) proposed_schedule[a] = -1;
        return;
    }

    // 3. 构建 AgentInfo / TaskInfo
    auto agents_info = make_agent_infos(free_agents_, env);  // 引用传递内部数据
    auto tasks_info  = make_task_infos(free_tasks_, env);

    // 4. 调用 matcher（内部自动处理区域划分 + 稀疏建边 + 匹配）
    MatchResult matches = matcher_.match(agents_info, tasks_info, deadline);

    // 5. 应用结果
    for (auto& [aid, tid] : matches) {
        proposed_schedule[aid] = tid;
        free_agents_.erase(aid);
        free_tasks_.erase(tid);
    }

    // 6. 未匹配 → -1
    for (int a : free_agents_) proposed_schedule[a] = -1;
}
```

**注意**：`OptScheduler::schedule()` 的代码和现在几乎一样，唯一区别是 `TaskMatcher::match()` 内部变了。

## 8. 配置参数

### Config.h 新增

```cpp
// ============ 可见范围划分（与拥塞 grid 独立） ============
constexpr int   TARGET_VISIBLE_TASKS    = 500;   // 每 agent 目标可见任务数
constexpr int   VISIBILITY_FINE_FACTOR  = 3;     // 细分因子
constexpr bool  VISIBILITY_FALLBACK     = true;  // 无可见 task 时全局保底
```

### RuntimeConfig.h 对应新增

```cpp
int   target_visible_tasks;    // OPT_TARGET_VISIBLE_TASKS
int   visibility_fine_factor;  // OPT_VISIBILITY_FINE_FACTOR
bool  visibility_fallback;     // OPT_VISIBILITY_FALLBACK
```

## 9. 文件改动清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `optsvr/GridPartition.h` | **新建** | 通用 grid 划分工具类 |
| `optsvr/GridPartition.cpp` | **新建** | 实现 |
| `optsvr/TaskMatcher.h` | **修改** | 新增 GridPartition 成员、稀疏边结构、内部方法 |
| `optsvr/TaskMatcher.cpp` | **重写** | 内部改为：建 grid 索引 → 稀疏建边 → 稀疏匹配 |
| `optsvr/Config.h` | 修改 | 新增 3 个可见范围参数 |
| `optsvr/RuntimeConfig.h` | 修改 | 新增 3 个运行时参数 |
| `optsvr/OptScheduler.h` | 修改 | 拥塞 grid 改用 GridPartition |
| `optsvr/OptScheduler.cpp` | 小改 | 拥塞部分改用 GridPartition（schedule 流程基本不变） |

## 10. 调用链层次验证（符合规则三：不超过 3 层）

```
OptScheduler::schedule()                         [第 1 层]
  └─ TaskMatcher::match()                        [第 2 层]
       ├─ build_bipartite_graph()                [第 3 层] — 建 grid 索引 + 打分建边
       └─ sparse_regret_match() / sparse_greedy_match()  [第 3 层] — 匹配
```

✅ 入口到执行逻辑最多 3 层。
