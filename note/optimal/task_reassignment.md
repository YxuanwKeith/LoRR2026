# 任务重分配（Task Reassignment）

## 一、核心观察

系统中存在两类 agent：
- **free agent**：刚完成任务 / 刚初始化，在 `new_freeagents` 中，等待被分配
- **en-route agent**：已分配了任务，但还没到达任务起点（`idx_next_loc == 0`），仍在赶路

**关键事实：切换任务零成本。** 因为 `Entry::compute()` 中执行顺序是 `scheduler->plan()` → `update_goal_locations()` → `planner->plan()`，路径规划永远在任务分配之后，planner 每轮根据最新的分配结果重新规划路径。被切换的 agent 不会有"已规划路径浪费"的问题。

因此，所有 `idx_next_loc == 0` 的 agent 本质上和 free agent 没有区别——它们持有的任务可以被自由重新分配。

---

## 二、可重分配条件（系统约束）

根据 `TaskManager::validate_task_assignment`（TaskManager.cpp:48, 57）：
- 任务 `idx_next_loc > 0`（已到达第一个 errand / "已打开"）→ **不可重分配**，必须由原 agent 继续
- 任务 `idx_next_loc == 0`（agent 还在赶路去起点）→ **可以被重新分配给其他 agent**

---

## 三、方案对比

### 方案 A：全局重匹配（理论最优，开销大）

每轮把所有 `idx_next_loc == 0` 的 en-route agent 都视为 free agent，连同它们持有的 task 一起放入匹配池，做一次全局最优匹配。

- **优点**：全局最优，不会遗漏任何更好的分配
- **缺点**：规模爆炸。例如 200 agent 场景中可能只有 5~10 个新 free agent，但有 180 个 en-route agent。全局重匹配 = 190 agents × 数百 tasks 的代价矩阵，每轮都要做，时间无法接受

### 方案 B：单向抢占（简单，但不一定最优）

每个 free agent 扫描所有 en-route agent 的任务，如果距离更近就抢。

- **优点**：简单，O(free × stealable)
- **缺点**：只考虑了 free agent 抢别人的任务是否更优，没有考虑被抢 agent 拿到新任务后的整体效果。可能出现局部最优但全局不优的情况

### ✅ 方案 C：局部重匹配（推荐方案）

**核心思想**：以每个新 free agent 为中心，找出其附近的 en-route agent 群，把这个局部群体连同它们的任务 + 未分配的 free tasks 一起做一次**小规模重匹配**。

**为什么这个平衡点好**：
- 只有新 free agent 的出现才可能触发更优分配（en-route agent 之间的相对位置不会突变）
- 重匹配范围限制在 free agent 附近，规模可控
- 涵盖了双向优化：free agent 可能拿到 en-route 的任务，en-route 也可能换到更优的 free task

---

## 四、方案 C 详细设计

### 在调度流程中的位置

```
schedule() {
    // 1. 收集 new_freeagents → free_agents_
    // 2. 收集 new_tasks → free_tasks_

    // ★ 新增步骤 2.5: 局部重匹配
    if (ENABLE_REASSIGNMENT && !free_agents_.empty()) {
        local_reassign(proposed_schedule, env, deadline);
    }

    // 3. 正常匹配剩余 free_agents × free_tasks
}
```

### `local_reassign()` 核心逻辑

```
local_reassign(proposed_schedule, env, deadline):
    reassign_deadline = now + time_limit * REASSIGN_TIME_BUDGET

    // 1. 预处理：收集所有可重分配的 en-route agent
    enroute_map = {}  // agent_id -> task_id
    for each agent a in [0, num_agents):
        task_id = env->curr_task_schedule[a]
        if task_id != -1
           && task_pool[task_id].idx_next_loc == 0
           && a not in free_agents_:
            enroute_map[a] = task_id

    // 2. 对每个新 free agent，构建局部重匹配子问题
    for each free_agent f in new_freeagents:  // 只遍历本轮新增的
        if now > reassign_deadline: break

        f_loc = env->curr_states[f].location

        // 2a. 找出 f 附近半径 R 内的 en-route agents
        //     "附近" 的定义：en-route agent 当前位置与 f 的曼哈顿距离 ≤ REASSIGN_RADIUS
        //     或者用 grid 区域快速筛选
        nearby_enroute = []
        for each (a, task_id) in enroute_map:
            if manhattan_dist(f_loc, env->curr_states[a].location) <= REASSIGN_RADIUS:
                nearby_enroute.append(a)

        if nearby_enroute.empty(): continue

        // 2b. 构建局部 agent 池和 task 池
        local_agents = [f] + nearby_enroute               // f 自己 + 附近 en-route
        local_tasks  = [enroute_map[a] for a in nearby_enroute]  // en-route 持有的 tasks
        
        // 额外加入一些 free_tasks 中距离较近的（给被换下的 agent 备选）
        nearby_free_tasks = top_K_nearest(free_tasks_, f_loc, K=len(local_agents))
        local_tasks += nearby_free_tasks
        
        local_tasks = deduplicate(local_tasks)

        // 2c. 小规模最优匹配
        //     local_agents 通常 5~20 个，local_tasks 10~40 个
        //     可以直接用 regret-based 匹配甚至匈牙利
        cost_matrix = compute_scores(local_agents, local_tasks, env)
        assignment = regret_match(cost_matrix)  // 或 hungarian

        // 2d. 对比原方案 vs 新方案，只接受严格更优的
        old_total_cost = sum(score(a, original_task[a]) for a in local_agents)
        new_total_cost = sum(cost_matrix[i][assignment[i]] for i)
        
        if new_total_cost < old_total_cost:
            // 应用新分配
            for each (agent, new_task) in assignment:
                if new_task in enroute_map.values():
                    // 这个 task 从某个 en-route agent 转移过来
                    proposed_schedule[agent] = new_task
                elif new_task in free_tasks_:
                    proposed_schedule[agent] = new_task
                    free_tasks_.erase(new_task)
            
            // 更新状态
            for a in nearby_enroute:
                if a 被分配到不同的任务:
                    enroute_map[a] = new_task  // 更新映射
                if a 未被分配到任何任务:
                    free_agents_.insert(a)     // 变为 free
                    enroute_map.erase(a)
            
            if f 被分配了任务:
                free_agents_.erase(f)
```

### 复杂度分析

设：
- F = 本轮新 free agent 数（通常 5~20）
- E = 每个 free agent 附近的 en-route 数（通常 5~15，由 REASSIGN_RADIUS 控制）
- K = 额外候选 free task 数

每个局部子问题规模：`(1 + E) agents × (E + K) tasks` ≈ 15 × 25

总时间：`F × O((1+E) × (E+K))` ≈ 10 × 375 = 3750 次打分

对比全局重匹配：`190 × 300 = 57000` 次打分 → 局部方案节省 **15 倍**

---

## 五、关键设计点

### 1. REASSIGN_RADIUS — 局部范围控制

用曼哈顿距离或 grid 区域筛选 en-route agent。太大 → 接近全局重匹配，太小 → 遗漏有价值的重分配。

建议值：**地图对角线的 15~25%**，或固定 **30~50 步**。也可以用 grid 坐标：与 free agent 同一 grid 或相邻 grid 内的 en-route agent。

### 2. 只遍历本轮新增的 free agent

en-route agent 之间的相对位置每轮变化不大（各自移动 1 步），不需要每轮对所有 en-route 做全局重匹配。只有新 free agent 的出现才引入了新的"更优分配"可能性。

### 3. 接受条件：整体更优

不是单看一对一的 gain，而是对比局部子问题的**总 cost** 新旧方案。这保证了重分配后所有参与者（包括被换下的 agent）整体更优，避免了"抢了别人任务但被抢者反而更差"的问题。

### 4. free tasks 补充

往局部 task 池中加入 free tasks，这样：
- 如果 free agent 距离某个 free task 最近 → 正常拿 free task，en-route 不变（退化为普通分配）
- 如果 free agent 距离某个 en-route 的 task 更近 → 发生重分配，en-route 拿 free task 作为替代
- 全面覆盖各种最优情况

### 5. 不需要改 TaskManager

所有操作都在 `proposed_schedule` 上进行，且只涉及 `idx_next_loc == 0` 的任务，完全符合系统验证逻辑。

### 6. 时间控制

设 `REASSIGN_TIME_BUDGET`（如 time_limit 的 20%），超时即停，不影响后续正常匹配。

---

## 六、新增参数

在 `optsvr/Config.h` 中新增：

```cpp
// ─── 任务重分配 ───
constexpr bool  ENABLE_REASSIGNMENT     = true;   // 是否启用局部重分配
constexpr int   REASSIGN_RADIUS         = 40;     // 局部重匹配的曼哈顿距离半径
constexpr float REASSIGN_TIME_BUDGET    = 0.2f;   // 重分配阶段占 time_limit 的比例上限
constexpr int   REASSIGN_FREE_TASK_K    = 10;     // 每个局部子问题额外加入的 free task 候选数
```

---

## 七、预期效果

| 场景 | 预期收益 | 原因 |
|------|---------|------|
| 城市/开放地图 | **高** | agent 分散，free agent 可能恰好在某个 en-route 目标附近 |
| 仓库/密集地图 | **中** | agent 密度高，局部重匹配频繁触发但 gain 适中 |
| 大规模场景（5000 agent）| **中** | 每轮新 free agent 较多，但 RADIUS 控制每次子问题规模 |
| 初始阶段（第一轮）| **无** | 所有 agent 同时为 free，直接全局匹配，不需要重分配 |

---

## 八、与现有匹配流程的关系

```
schedule() {
    1. 收集 free_agents_, free_tasks_
    2. 更新拥塞热力图
    
    ★ 2.5 局部重分配 (local_reassign)
       - 输入: free_agents_ (本轮新增), en-route agents, free_tasks_
       - 效果: 部分 free agent 拿到 en-route 的任务, 部分 en-route 变为 free
       - 更新: free_agents_, free_tasks_, proposed_schedule
    
    3. 正常匹配 (hungarian / greedy_topk)
       - 处理 local_reassign 后剩余的 free_agents_ × free_tasks_
       - 包括被换下的原 en-route agent（它们现在在 free_agents_ 中）
}
```

重分配阶段是一个**无损的前置优化**：
- 如果找到更优分配 → 应用，被换下的 agent 进入 free 池参与正常匹配
- 如果找不到更优分配 → 不做任何改变，零影响
- 最坏情况 = 浪费了一点计算时间（由 TIME_BUDGET 控制上限）
