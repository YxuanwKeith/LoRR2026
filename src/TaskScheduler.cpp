#include "TaskScheduler.h"

#include "scheduler.h"
#include "const.h"

// 优化调度器
#include "RuntimeConfig.h"
#include "OptScheduler.h"

// 全局优化调度器实例（非 static，供 Combined 赛道 Entry.cpp 通过 extern 访问）
optsvr::OptScheduler g_opt_scheduler;
static bool g_config_loaded = false;

/**
 * Initializes the task scheduler with a given time limit for preprocessing.
 */
void TaskScheduler::initialize(int preprocess_time_limit)
{
    // 加载运行时配置（从环境变量，无则用默认值）
    if (!g_config_loaded) {
        optsvr::cfg().load_from_env();
        g_config_loaded = true;
    }

    //give at most half of the entry time_limit to scheduler;
    //-SCHEDULER_TIMELIMIT_TOLERANCE for timing error tolerance
    int limit = preprocess_time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;

    // 默认调度器的初始化（heuristic 表等）始终需要
    DefaultPlanner::schedule_initialize(limit, env);

    // 优化调度器的初始化
    if (optsvr::cfg().use_opt_scheduler) {
        g_opt_scheduler.initialize(env);
    }
}

/**
 * Plans a task schedule within a specified time limit.
 */
void TaskScheduler::plan(int time_limit, std::vector<int> & proposed_schedule)
{
    //give at most half of the entry time_limit to scheduler;
    //-SCHEDULER_TIMELIMIT_TOLERANCE for timing error tolerance
    int limit = time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;

    if (optsvr::cfg().use_opt_scheduler) {
        g_opt_scheduler.schedule(limit, proposed_schedule, env);
    } else {
        DefaultPlanner::schedule_plan(limit, proposed_schedule, env);
    }
}
