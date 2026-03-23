// optsvr/Config.h — 编译期默认参数（提交评测时使用这些值）
#pragma once

namespace optsvr {

// ============ 主开关 ============
constexpr bool  USE_OPT_SCHEDULER       = true;    // false = baseline

// ============ 打分模式 ============
// 0 = 匈牙利 + score = dist_to_start（只看距离，消融最优）
// 1 = 匈牙利 + score = dist_to_start + task_length
// 2 = 匈牙利 + 距离比例评分（新思路：系数 = len/avg_len，后期直接加 len）
// 3 = 匈牙利 + dist + difficulty_weight * (task_len + congestion)
constexpr int   SCORE_MODE              = 0;       // 默认 dist_only（消融最优）

// ============ 匹配策略 ============
constexpr int   HUNGARIAN_THRESHOLD     = 300;     // free_agents 超过此值改用区域匹配
constexpr int   CANDIDATE_TOPK          = 50;      // 大规模时每个 agent 的候选任务数
constexpr bool  USE_HUNGARIAN           = true;    // true=小规模用 KM 匈牙利; false=用 regret 贪心

// ============ 拥塞热力图 ============
constexpr float CONGESTION_DECAY        = 0.9f;    // 衰减因子
constexpr int   CONGESTION_TTL          = 100;     // 时效（步数）

// ============ 任务难度 ============
constexpr float DIFFICULTY_WEIGHT       = 0.3f;    // 难度在评分中的权重

// ============ 区域划分（Grid-based 拥塞） ============
constexpr int   REGION_SIZE             = 4;       // 向后兼容（已废弃，改用 grid 方案）
constexpr int   CONGESTION_GRID_CELL    = 4;       // Grid 分块大小（4×4 个 cell 为一个 grid）
constexpr int   CONGESTION_GRID_RADIUS  = 1;       // 查询时考虑的 grid 半径（半径=0 → 仅当前 grid，半径=1 → 3×3 grids）
constexpr bool  CONGESTION_USE_HISTORY  = false;   // true=累积历史衰减; false=每轮只用当前 agent 分布（当前最优）

// ============ 模拟总步数（用于新思路后期切换） ============
constexpr int   TOTAL_STEPS             = 1000;    // 模拟总步数（从命令行 -s 传入）

} // namespace optsvr
