// optsvr/TaskMatcher.h — 任务匹配器（纯匹配逻辑，无状态）
// 传入一批 agent 和 task，输出最优匹配结果
#pragma once

#include "SharedEnv.h"
#include "heuristics.h"
#include "RuntimeConfig.h"
#include <vector>
#include <chrono>
#include <functional>
#include <limits>

namespace optsvr {

// 匹配结果：(agent_id, task_id) 的配对列表
using MatchResult = std::vector<std::pair<int, int>>;

// ================================================================
// AgentInfo — 参与匹配的 agent 描述
// ================================================================
struct AgentInfo {
    int id;       // agent 编号
    int location; // 当前位置（cell 编号）
};

// ================================================================
// TaskInfo — 参与匹配的 task 描述
// ================================================================
struct TaskInfo {
    int id;                    // task 编号
    int start_location;        // 任务起点（idx_next_loc 对应的 location）
    std::vector<int> errand_locations; // 剩余 errand 的位置列表（从 idx_next_loc 开始）
};

// ================================================================
// ScoreFunc — 打分函数类型
//   接受 (agent_index, task_index)，返回 score（越低越好）
// ================================================================
using ScoreFunc = std::function<float(int, int)>;

// ================================================================
// TaskMatcher — 无状态任务匹配器
//
// 使用方式：
//   TaskMatcher matcher;
//   matcher.set_env(env);                    // 设置环境（用于 heuristic 和拥塞）
//   matcher.set_congestion_func(func);       // 可选：设置拥塞查询函数
//   auto result = matcher.match(agents, tasks, deadline);
//
// 也可以自定义打分函数：
//   auto result = matcher.match(agents, tasks, score_func, deadline);
// ================================================================
class TaskMatcher {
public:
    // 设置环境指针（用于 heuristic 距离计算）
    void set_env(SharedEnvironment* env) { env_ = env; }

    // 设置拥塞查询函数（可选，不设置则不考虑拥塞）
    // 函数签名：float(int location) -> 该位置的拥塞度
    void set_congestion_func(std::function<float(int)> func) {
        congestion_func_ = std::move(func);
    }

    // 设置当前时间步和 free tasks 的平均任务长度（供 score_mode=2 使用）
    void set_context(int curr_timestep, float avg_task_length) {
        curr_timestep_ = curr_timestep;
        avg_task_length_ = avg_task_length;
    }

    // ============ 核心接口 ============

    // 使用内置打分函数进行匹配
    MatchResult match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        std::chrono::steady_clock::time_point deadline);

    // 使用自定义打分函数进行匹配
    MatchResult match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const ScoreFunc& score_func,
        std::chrono::steady_clock::time_point deadline);

    // ============ 打分函数（可供外部单独调用） ============

    // mode 0 打分：score = dist_to_start（只看 agent 到任务起点的距离）
    float score_dist_only(const AgentInfo& agent, const TaskInfo& task) const;

    // mode 1 打分：score = dist_to_start + task_length（距离 + 任务自身长度）
    float score(const AgentInfo& agent, const TaskInfo& task) const;

    // mode 2 打分：距离比例评分（新思路）
    //   正常期：dist_to_start + (len / avg_len) * task_length
    //   后期（剩余 tick <= 2 * avg_len）：dist_to_start + task_length
    float score_mode2(const AgentInfo& agent, const TaskInfo& task) const;

    // mode 3 打分：dist_to_start + difficulty_weight * (task_length + congestion)
    //   之前带权重和拥塞惩罚的方案
    float score_weighted(const AgentInfo& agent, const TaskInfo& task) const;

private:
    SharedEnvironment* env_ = nullptr;
    std::function<float(int)> congestion_func_ = nullptr;
    int curr_timestep_ = 0;
    float avg_task_length_ = 1.0f;

    // ---- 匈牙利（KM）带权最优匹配（小规模） ----
    // 在 N×M 代价矩阵上求最小权完美匹配（N <= M）
    // 复杂度 O(N^2 * M)
    MatchResult hungarian_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const ScoreFunc& score_func,
        std::chrono::steady_clock::time_point deadline);

    // ---- Regret-based 匹配（匈牙利的 fallback） ----
    MatchResult regret_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const ScoreFunc& score_func,
        std::chrono::steady_clock::time_point deadline);

    // ---- Baseline 风格贪心匹配 ----
    // 逐个 agent 选 makespan 最小的 task，和默认调度器完全一致
    MatchResult baseline_greedy_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        std::chrono::steady_clock::time_point deadline);

    // ---- TopK 贪心匹配（大规模快速） ----
    MatchResult greedy_topk_match(
        const std::vector<AgentInfo>& agents,
        const std::vector<TaskInfo>& tasks,
        const ScoreFunc& score_func,
        std::chrono::steady_clock::time_point deadline);
};

} // namespace optsvr
