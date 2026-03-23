// optsvr/OptScheduler.h — 优化任务调度器
#pragma once

#include "SharedEnv.h"
#include "RuntimeConfig.h"
#include "TaskMatcher.h"
#include <vector>
#include <chrono>

namespace optsvr {

class OptScheduler {
public:
    void initialize(SharedEnvironment* env);
    void schedule(int time_limit, std::vector<int>& proposed_schedule, SharedEnvironment* env);

    TaskMatcher& matcher() { return matcher_; }
    const TaskMatcher& matcher() const { return matcher_; }

private:
    // 地图基本信息
    int map_size_ = 0;
    int rows_ = 0;
    int cols_ = 0;

    // ---- Grid-based 拥塞系统 ----
    int grid_rows_ = 0;
    int grid_cols_ = 0;
    int grid_count_ = 0;
    std::vector<float> grid_congestion_;

    int cell_to_grid(int loc) const;

    // ---- 核心步骤 ----
    void build_grid_layout();
    void update_congestion(SharedEnvironment* env);
    float get_region_congestion(int loc) const;

    // ---- 匹配器 ----
    TaskMatcher matcher_;

    // ---- 辅助 ----
    AgentInfo make_agent_info(int agent_id, SharedEnvironment* env) const;
    TaskInfo  make_task_info(int task_id, SharedEnvironment* env) const;
};

} // namespace optsvr
