#include "Entry.h"
#include "Tasks.h"
#include "utils.h"
#include "heuristics.h"

// The initialize function will be called by competition system at the preprocessing stage.
void Entry::initialize(int preprocess_time_limit)
{
    scheduler->initialize(preprocess_time_limit);
    planner->initialize(preprocess_time_limit);
}

void Entry::compute(int time_limit, Plan & plan, std::vector<int> & proposed_schedule)
{
    //call the task scheduler to assign tasks to agents
    auto t0 = std::chrono::steady_clock::now();
    scheduler->plan(time_limit,proposed_schedule);
    auto t1 = std::chrono::steady_clock::now();

    //then update the first unfinished errand/location of tasks for planner reference
    update_goal_locations(proposed_schedule);
    
    //call the planner to compute the actions
    planner->plan(time_limit,plan);
    auto t2 = std::chrono::steady_clock::now();

    // 记录本轮任务分配和路径规划各自的耗时
    env->last_schedule_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    env->last_plan_ms     = std::chrono::duration<double, std::milli>(t2 - t1).count();
}

// Set the next goal locations for each agent based on the proposed schedule
void Entry::update_goal_locations(std::vector<int> & proposed_schedule)
{
    // record the proposed schedule so that we can tell the planner
    env->curr_task_schedule = proposed_schedule;

    for (size_t i = 0; i < proposed_schedule.size(); i++)
    {
        env->goal_locations[i].clear();
        int t_id = proposed_schedule[i];
        if (t_id == -1)
            continue;

        int i_loc = env->task_pool[t_id].idx_next_loc;
        env->goal_locations[i].push_back({env->task_pool[t_id].locations.at(i_loc), env->task_pool[t_id].t_revealed});
    }
}