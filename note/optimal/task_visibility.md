# Task Visibility — Agent 可见任务范围方案

## 1. 问题

当前 `schedule()` 将 **所有** free tasks 传给 `TaskMatcher`，在大规模算例中：
- warehouse_large: 5000 agents，numTasksReveal=1.5 → 每轮可能有数千 free tasks
- regret_match: 构建 N×M 代价矩阵，5000×15000 = 7500万次打分
- greedy_topk: 每个 agent 遍历所有 task 做曼哈顿筛选，5000×数千 = 千万次

**目标**：agent 只看到其「所在区域附近」的 tasks，将 N×M 降为 N×m（m << M）。

## 2. 核心设计

### 2.1 复用已有 Grid 划分

已有 `grid_cell_size` 将地图分为 `grid_rows_ × grid_cols_` 的 grid 块。新增一个 **task_visibility_radius** 参数，含义：

> agent 只能看到其所在 grid **±** visibility_radius 范围内的 grid 中的 tasks

例如 visibility_radius=2 → agent 看到 5×5 = 25 个 grid 中的 tasks。

### 2.2 新增组件：GridTaskIndex

维护一个 **grid → tasks** 的反向索引，每轮更新：

```
// 数据结构
std::vector<std::vector<int>> grid_tasks_;  // grid_id -> [task_id, ...]

// 每轮构建
void build_task_index(const std::unordered_set<int>& free_tasks, SharedEnvironment* env) {
    for (auto& v : grid_tasks_) v.clear();
    for (int task_id : free_tasks) {
        int start_loc = env->task_pool[task_id].locations[env->task_pool[task_id].idx_next_loc];
        int grid_id = cell_to_grid(start_loc);
        grid_tasks_[grid_id].push_back(task_id);
    }
}
```

### 2.3 查询可见任务

```
// 获取 agent 所在 grid ± visibility_radius 内的所有 tasks
std::vector<int> get_visible_tasks(int agent_loc) const {
    int radius = cfg().task_visibility_radius;
    int agent_grid_r = ...; // agent 所在 grid 行
    int agent_grid_c = ...; // agent 所在 grid 列
    
    std::vector<int> result;
    for (r in [agent_grid_r - radius, agent_grid_r + radius]):
        for (c in [agent_grid_c - radius, agent_grid_c + radius]):
            int gid = r * grid_cols_ + c;
            result.insert(result.end(), grid_tasks_[gid].begin(), grid_tasks_[gid].end());
    return result;
}
```

### 2.4 修改 schedule() 流程

**关键变化**：不再把全部 free tasks 传给 matcher，而是根据 agent 可见范围分组。

**方案 A — 按 agent 独立可见范围**（最精确，但多个 agent 可能重复选同一 task）：
```
for each free_agent a:
    visible_tasks = get_visible_tasks(a.location)
    // 只对 visible_tasks 打分
```

**方案 B — 按 grid 分组匹配**（更高效，天然无冲突）：
```
// 将 agents 按所在 grid 分组
// 将 tasks 按起点 grid 分组
// 对每个 grid 及其邻近区域，收集 local_agents 和 local_tasks，调用一次 matcher.match()
```

**推荐方案 B**，原因：
1. 每组规模小（5000 agents / 100 grids ≈ 50 agents/grid），匹配极快
2. 不同组之间天然没有冲突（agent 被唯一分到一个 grid）
3. 复用 TaskMatcher 无需改动
4. 边界 agent 通过 visibility_radius 看到邻近 grid 的 task，不会丢失好选项

### 2.5 方案 B 详细流程

```
schedule() {
    // ... 收集 free_agents, free_tasks, 更新拥塞 ...
    
    // 1. 构建 task grid index
    build_task_index();
    
    // 2. 将 agents 按所在 grid 分组
    std::vector<std::vector<int>> grid_agents(grid_count_);
    for (int a : free_agents_) {
        int gid = cell_to_grid(env->curr_states[a].location);
        grid_agents[gid].push_back(a);
    }
    
    // 3. 逐 grid 组匹配
    for (int gid = 0; gid < grid_count_; gid++) {
        if (grid_agents[gid].empty()) continue;
        
        // 收集 local_agents
        auto local_agents = build_agent_infos(grid_agents[gid], env);
        
        // 收集 visible_tasks：当前 grid ± visibility_radius 内的所有 free tasks
        auto local_tasks = collect_visible_tasks(gid);
        
        if (local_tasks.empty()) continue;
        
        // 调用 matcher（小规模子问题）
        auto matches = matcher_.match(local_agents, local_tasks, sub_deadline);
        
        // 应用结果
        apply_matches(matches, proposed_schedule);
    }
    
    // 4. 兜底：可见范围内没有 task 的 agent → 扩大搜索或等下轮
    handle_unmatched_agents(proposed_schedule, env, deadline);
}
```

### 2.6 边界处理

1. **可见范围内没有 task 的 agent**：
   - 方案 a: 扩大该 agent 的 visibility_radius（如 ×2）重新搜索
   - 方案 b: 将其加入一个 `unmatched_agents` 列表，最后对这些 agent 做一次全局匹配
   - 推荐方案 b，简单可控

2. **visibility_radius 过小导致 task 竞争激烈**：
   - 适当增大 visibility_radius，使每个 agent 平均可见 task 数 ≈ 候选 TopK 数（50~100）
   - 可根据 `free_tasks.size() / grid_count_` 自适应调整

3. **task 被多个 grid 组的 agent 竞争**：
   - 按 grid 顺序匹配，已分配的 task 从 `free_tasks_` 移除
   - 后续 grid 的 visible_tasks 自然不包含已分配的 task（动态过滤）

## 3. 新增配置参数

| 参数 | 编译期默认 | 含义 |
|------|-----------|------|
| `TASK_VISIBILITY_RADIUS` | 3 | agent 可见 task 的 grid 半径（半径=3 → 看到 7×7 = 49 个 grid） |
| `VISIBILITY_FALLBACK_GLOBAL` | true | 可见范围内无 task 时是否回退到全局搜索 |

## 4. 复杂度分析

假设 5000 agents, 10000 tasks, 256×256 地图, grid_cell=4 → 64×64=4096 grids

### 当前（无 visibility）
- regret_match: 5000 × 10000 = 5000万次打分 ❌
- greedy_topk: 5000 × 10000 遍历 + 5000 × 50 打分 = 5025万 ❌

### 方案 B（visibility_radius=3, 7×7=49 grids/组）
- 平均每 grid: 5000/4096 ≈ 1.2 agents, 10000/4096 ≈ 2.4 tasks
- 每组: ~1.2 agents × (2.4 × 49) ≈ 1.2 × 118 ≈ 142 次打分
- 总计: ~4096 × 142 ≈ 58万次打分 ✅（降低 ~86倍）

实际上由于 agent 和 task 分布不均（warehouse 走廊密集），热点 grid 可能有 30-50 agents，
但即使如此：50 agents × 200 visible_tasks = 10000 次打分 × 少数热点 grid → 仍远小于全局。

## 5. 与后续功能的关系

### 任务重分配（task_reassignment）
- 局部重匹配方案天然适配：free agent 所在 grid 附近的 en-route agent 正是同一可见区域
- `get_visible_tasks()` 直接复用

### 拥塞系统
- grid 划分完全复用现有 `grid_congestion_`，不增加额外数据结构

### 代码改动范围
- **新增**：`GridTaskIndex` 类（或嵌入 OptScheduler）
- **修改**：`OptScheduler::schedule()` 流程改为逐 grid 组匹配
- **不修改**：`TaskMatcher`（接口不变，只是传入的 tasks 变少了）
- **新增配置**：`Config.h` + `RuntimeConfig.h` 各加 2 个参数
