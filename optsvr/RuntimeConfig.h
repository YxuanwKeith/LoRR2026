// optsvr/RuntimeConfig.h — 运行时参数配置
// 支持通过环境变量覆盖编译期默认值，无需重新编译
// 提交评测时环境变量不存在，自动回退到 Config.h 中的 constexpr 默认值
#pragma once

#include <cstdlib>
#include <string>
#include <iostream>
#include "Config.h"

namespace optsvr {

struct RuntimeConfig {
    // ============ 参数字段 ============
    bool  use_opt_scheduler;
    int   score_mode;           // 0=baseline贪心, 1=匈牙利dist+len, 2=匈牙利距离比例
    int   hungarian_threshold;
    int   candidate_topk;
    bool  use_hungarian;        // true=小规模用 KM 匈牙利; false=用 regret 贪心
    float congestion_decay;
    int   congestion_ttl;
    float difficulty_weight;
    int   region_size;
    int   grid_cell_size;       // Grid 分块大小（N×N cell = 1 grid）
    int   grid_radius;          // 拥塞查询时考虑的 grid 半径
    bool  congestion_use_history; // true=累积历史衰减; false=每轮只用当前快照
    int   total_steps;          // 模拟总步数（用于新思路后期切换）

    // ============ 单例访问 ============
    static RuntimeConfig& instance() {
        static RuntimeConfig cfg;
        return cfg;
    }

    // 在 initialize 阶段调用一次，从环境变量加载参数
    void load_from_env() {
        use_opt_scheduler   = read_bool("OPT_USE_OPT_SCHEDULER",   USE_OPT_SCHEDULER);
        score_mode          = read_int ("OPT_SCORE_MODE",           SCORE_MODE);
        hungarian_threshold = read_int ("OPT_HUNGARIAN_THRESHOLD",  HUNGARIAN_THRESHOLD);
        candidate_topk      = read_int ("OPT_CANDIDATE_TOPK",       CANDIDATE_TOPK);
        use_hungarian       = read_bool("OPT_USE_HUNGARIAN",         USE_HUNGARIAN);
        congestion_decay    = read_float("OPT_CONGESTION_DECAY",    CONGESTION_DECAY);
        congestion_ttl      = read_int ("OPT_CONGESTION_TTL",       CONGESTION_TTL);
        difficulty_weight   = read_float("OPT_DIFFICULTY_WEIGHT",   DIFFICULTY_WEIGHT);
        region_size         = read_int ("OPT_REGION_SIZE",          REGION_SIZE);
        grid_cell_size      = read_int ("OPT_GRID_CELL_SIZE",       CONGESTION_GRID_CELL);
        grid_radius         = read_int ("OPT_GRID_RADIUS",          CONGESTION_GRID_RADIUS);
        congestion_use_history = read_bool("OPT_CONGESTION_USE_HISTORY", CONGESTION_USE_HISTORY);
        total_steps         = read_int ("OPT_TOTAL_STEPS",          TOTAL_STEPS);

        print_config();
    }

private:
    RuntimeConfig() {
        // 默认用编译期常量初始化
        use_opt_scheduler   = USE_OPT_SCHEDULER;
        score_mode          = SCORE_MODE;
        hungarian_threshold = HUNGARIAN_THRESHOLD;
        candidate_topk      = CANDIDATE_TOPK;
        use_hungarian       = USE_HUNGARIAN;
        congestion_decay    = CONGESTION_DECAY;
        congestion_ttl      = CONGESTION_TTL;
        difficulty_weight   = DIFFICULTY_WEIGHT;
        region_size         = REGION_SIZE;
        grid_cell_size      = CONGESTION_GRID_CELL;
        grid_radius         = CONGESTION_GRID_RADIUS;
        congestion_use_history = CONGESTION_USE_HISTORY;
        total_steps         = TOTAL_STEPS;
    }

    // ============ 环境变量读取辅助函数 ============
    static bool read_bool(const char* name, bool default_val) {
        const char* val = std::getenv(name);
        if (!val) return default_val;
        std::string s(val);
        return (s == "1" || s == "true" || s == "True" || s == "TRUE");
    }

    static int read_int(const char* name, int default_val) {
        const char* val = std::getenv(name);
        if (!val) return default_val;
        try { return std::stoi(val); }
        catch (...) { return default_val; }
    }

    static float read_float(const char* name, float default_val) {
        const char* val = std::getenv(name);
        if (!val) return default_val;
        try { return std::stof(val); }
        catch (...) { return default_val; }
    }

    void print_config() const {
        std::cerr << "[optsvr] RuntimeConfig loaded:\n"
                  << "  use_opt_scheduler   = " << use_opt_scheduler << "\n"
                  << "  score_mode          = " << score_mode << "\n"
                  << "  hungarian_threshold = " << hungarian_threshold << "\n"
                  << "  candidate_topk      = " << candidate_topk << "\n"
                  << "  use_hungarian       = " << use_hungarian << "\n"
                  << "  congestion_decay    = " << congestion_decay << "\n"
                  << "  congestion_ttl      = " << congestion_ttl << "\n"
                  << "  difficulty_weight   = " << difficulty_weight << "\n"
                  << "  region_size         = " << region_size << "\n"
                  << "  grid_cell_size      = " << grid_cell_size << "\n"
                  << "  grid_radius         = " << grid_radius << "\n"
                  << "  congestion_use_history = " << congestion_use_history << "\n"
                  << "  total_steps         = " << total_steps << "\n";
    }
};

// 便捷访问宏（使用时类似 optsvr::cfg().hungarian_threshold）
inline RuntimeConfig& cfg() { return RuntimeConfig::instance(); }

} // namespace optsvr
