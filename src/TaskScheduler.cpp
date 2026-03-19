#include "TaskScheduler.h"

#include "const.h"
#include "heuristics.h"
#include "task_assignment.h"

namespace {

std::vector<int> collect_unassigned_agents(const std::vector<int>& schedule)
{
    std::vector<int> free_agents;
    free_agents.reserve(schedule.size());
    for (int agent_id = 0; agent_id < static_cast<int>(schedule.size()); ++agent_id)
    {
        if (schedule[agent_id] == -1)
        {
            free_agents.push_back(agent_id);
        }
    }
    return free_agents;
}

std::vector<int> collect_available_tasks(const SharedEnvironment* env)
{
    std::vector<int> free_tasks;
    free_tasks.reserve(env->task_pool.size());
    for (const auto& [task_id, task] : env->task_pool)
    {
        if (task.idx_next_loc < static_cast<int>(task.locations.size()) && task.agent_assigned == -1)
        {
            free_tasks.push_back(task_id);
        }
    }
    return free_tasks;
}

} // namespace

/**
 * Initializes the task scheduler with a given time limit for preprocessing.
 * 
 * This function prepares the task scheduler by allocating up to half of the given preprocessing time limit 
 * and adjust for a specified tolerance to account for potential timing errors. 
 * It ensures that initialization does not exceed the allocated time.
 * 
 * @param preprocess_time_limit The total time limit allocated for preprocessing (in milliseconds).
 *
 */
void TaskScheduler::initialize(int preprocess_time_limit)
{
    //give at most half of the entry time_limit to scheduler;
    //-SCHEDULER_TIMELIMIT_TOLERANCE for timing error tolerance
    int limit = preprocess_time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;
    DefaultPlanner::init_heuristics(env);
    TaskAssignment::initialize_task_scoring(env);
    (void) limit;
}

/**
 * Plans a task schedule within a specified time limit.
 * 
 * This function schedules tasks by calling shedule_plan function in default planner with half of the given time limit,
 * adjusted for timing error tolerance. The planned schedule is output to the provided schedule vector.
 * 
 * @param time_limit The total time limit allocated for scheduling (in milliseconds).
 * @param proposed_schedule A reference to a vector that will be populated with the proposed schedule (next task id for each agent).
 */

void TaskScheduler::plan(int time_limit, std::vector<int> & proposed_schedule)
{
    //give at most half of the entry time_limit to scheduler;
    //-SCHEDULER_TIMELIMIT_TOLERANCE for timing error tolerance
    int limit = time_limit/2 - DefaultPlanner::SCHEDULER_TIMELIMIT_TOLERANCE;
    (void) limit;

    if (env == nullptr || env->num_of_agents <= 0)
    {
        return;
    }

    if (static_cast<int>(proposed_schedule.size()) != env->num_of_agents)
    {
        proposed_schedule.assign(env->num_of_agents, -1);
    }

    const std::vector<int> free_agents = collect_unassigned_agents(proposed_schedule);
    const std::vector<int> free_tasks = collect_available_tasks(env);
    if (free_agents.empty() || free_tasks.empty())
    {
        return;
    }

    const std::vector<std::pair<int, int>> assignments =
        TaskAssignment::solve_bipartite_matching(env, free_agents, free_tasks);

    for (const auto& [agent_id, task_id] : assignments)
    {
        proposed_schedule[agent_id] = task_id;
    }
}
