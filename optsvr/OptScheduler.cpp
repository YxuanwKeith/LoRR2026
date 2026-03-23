// optsvr/OptScheduler.cpp — 优化任务调度器实现
#include "OptScheduler.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <cassert>
#include <unordered_set>

namespace optsvr {

// ================================================================
// initialize — 预处理阶段调用一次
// ================================================================
void OptScheduler::initialize(SharedEnvironment* env) {
    map_size_ = env->rows * env->cols;
    rows_ = env->rows;
    cols_ = env->cols;

    // 构建 grid 划分
    build_grid_layout();

    // 确保 heuristic 表已初始化（DefaultPlanner 的全局数据）
    DefaultPlanner::init_heuristics(env);

    // 初始化匹配器
    matcher_.set_env(env);
    matcher_.set_congestion_func(
        [this](int loc) -> float { return this->get_region_congestion(loc); }
    );
}

// ================================================================
// build_grid_layout — 预处理：将地图划分为 N×N 的 grid
// ================================================================
void OptScheduler::build_grid_layout() {
    int cell_size = cfg().grid_cell_size;
    if (cell_size <= 0) cell_size = 4;

    grid_rows_ = (rows_ + cell_size - 1) / cell_size;
    grid_cols_ = (cols_ + cell_size - 1) / cell_size;
    grid_count_ = grid_rows_ * grid_cols_;
    grid_congestion_.assign(grid_count_, 0.0f);
}

// ================================================================
// cell_to_grid — cell 位置 → grid 编号
// ================================================================
int OptScheduler::cell_to_grid(int loc) const {
    int cell_size = cfg().grid_cell_size;
    if (cell_size <= 0) cell_size = 4;
    int row = loc / cols_;
    int col = loc % cols_;
    int gr = row / cell_size;
    int gc = col / cell_size;
    return gr * grid_cols_ + gc;
}

// ================================================================
// make_agent_info — 将 agent_id 转为 AgentInfo
// ================================================================
AgentInfo OptScheduler::make_agent_info(int agent_id, SharedEnvironment* env) const {
    return AgentInfo{agent_id, env->curr_states[agent_id].location};
}

// ================================================================
// make_task_info — 将 task_id 转为 TaskInfo
// ================================================================
TaskInfo OptScheduler::make_task_info(int task_id, SharedEnvironment* env) const {
    const Task& task = env->task_pool[task_id];
    TaskInfo info;
    info.id = task_id;
    info.start_location = task.locations[task.idx_next_loc];
    info.errand_locations.reserve(task.locations.size() - task.idx_next_loc);
    for (size_t i = task.idx_next_loc; i < task.locations.size(); i++) {
        info.errand_locations.push_back(task.locations[i]);
    }
    return info;
}

// ================================================================
// schedule — 每轮调用，分配 free agents 到 free tasks
//
// 流程：
// 1. 分类 agents，收集 assigned_tasks
// 2. 收集 free tasks
// 3. 更新拥塞热力图
// 4. 匹配 free agents ↔ free tasks
//
// 注意：proposed_schedule 是主线程/子线程无锁共享的，
// 子线程只能安全地写 proposed_schedule[i]==-1 的 slot（free agent）。
// ================================================================
void OptScheduler::schedule(int time_limit,
                            std::vector<int>& proposed_schedule,
                            SharedEnvironment* env) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(time_limit);

    int n_agents = env->num_of_agents;

    // ---- Step 1: 分类 agents，收集 assigned_tasks ----
    std::vector<int> free_agents;
    std::unordered_set<int> assigned_tasks;

    for (int i = 0; i < n_agents; i++) {
        int tid = proposed_schedule[i];
        if (tid == -1) {
            free_agents.push_back(i);
        } else {
            assigned_tasks.insert(tid);
        }
    }

    // ---- Step 2: 收集 free tasks ----
    std::vector<int> free_tasks;
    for (auto& [tid, task] : env->task_pool) {
        if (assigned_tasks.find(tid) == assigned_tasks.end()) {
            free_tasks.push_back(tid);
        }
    }

    // ---- Step 3: 更新拥塞热力图 ----
    update_congestion(env);

    // 无 free task 或无 free agent → 跳过匹配
    if (free_tasks.empty() || free_agents.empty()) return;

    // ---- Step 4: 构建匹配列表 ----
    std::vector<AgentInfo> agents_info;
    std::vector<TaskInfo>  tasks_info;

    agents_info.reserve(free_agents.size());
    for (int a : free_agents) agents_info.push_back(make_agent_info(a, env));

    tasks_info.reserve(free_tasks.size());
    for (int t : free_tasks) tasks_info.push_back(make_task_info(t, env));

    // ---- Step 4.5: 为 score_mode=2 计算 avg_task_length ----
    // avg_task_length = 所有 free task 的平均 makespan（errand 间距离之和）
    float avg_task_length = 1.0f;
    if (cfg().score_mode == 2 && !tasks_info.empty()) {
        constexpr int H_CAP = 100000;
        float total = 0.0f;
        for (const auto& ti : tasks_info) {
            int len = 0;
            if (ti.errand_locations.size() > 1) {
                int prev = ti.errand_locations[0];
                for (size_t k = 1; k < ti.errand_locations.size(); k++) {
                    int next = ti.errand_locations[k];
                    int h = DefaultPlanner::get_h(env, prev, next);
                    if (h >= H_CAP) h = H_CAP;
                    len += h;
                    prev = next;
                }
            }
            total += static_cast<float>(len);
        }
        avg_task_length = total / static_cast<float>(tasks_info.size());
        if (avg_task_length < 1.0f) avg_task_length = 1.0f;
    }
    matcher_.set_context(env->curr_timestep, avg_task_length);

    // ---- Step 5: 匹配 ----
    MatchResult matches = matcher_.match(agents_info, tasks_info, deadline);

    // ---- Step 6: 应用匹配结果 ----
    std::unordered_set<int> matched_agents;
    for (auto& [aid, tid] : matches) {
        proposed_schedule[aid] = tid;
        matched_agents.insert(aid);
    }

    // 未匹配的 free agent 保持 -1
    for (int a : free_agents) {
        if (!matched_agents.count(a)) proposed_schedule[a] = -1;
    }
}

// ================================================================
// update_congestion — 更新 grid 级拥塞热力图
// ================================================================
void OptScheduler::update_congestion(SharedEnvironment* env) {
    bool use_history = cfg().congestion_use_history;

    if (use_history) {
        float decay = cfg().congestion_decay;
        for (int i = 0; i < grid_count_; i++) {
            grid_congestion_[i] *= decay;
        }
    } else {
        std::fill(grid_congestion_.begin(), grid_congestion_.end(), 0.0f);
    }

    for (int i = 0; i < env->num_of_agents; i++) {
        int loc = env->curr_states[i].location;
        if (loc >= 0 && loc < map_size_) {
            int gid = cell_to_grid(loc);
            grid_congestion_[gid] += 1.0f;
        }
    }
}

// ================================================================
// get_region_congestion — 区域平均拥塞度（grid-based）
// ================================================================
float OptScheduler::get_region_congestion(int loc) const {
    int cell_size = cfg().grid_cell_size;
    if (cell_size <= 0) cell_size = 4;
    int radius = cfg().grid_radius;

    int row = loc / cols_;
    int col = loc % cols_;
    int gr = row / cell_size;
    int gc = col / cell_size;

    int gr_start = std::max(0, gr - radius);
    int gr_end   = std::min(grid_rows_ - 1, gr + radius);
    int gc_start = std::max(0, gc - radius);
    int gc_end   = std::min(grid_cols_ - 1, gc + radius);

    float sum = 0.0f;
    int count = 0;
    for (int r = gr_start; r <= gr_end; r++) {
        for (int c = gc_start; c <= gc_end; c++) {
            int gid = r * grid_cols_ + c;
            sum += grid_congestion_[gid];
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0f;
}

} // namespace optsvr
