#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "TaskDifficultyScheduler.h"

namespace {

SharedEnvironment build_nearest_task_env()
{
    SharedEnvironment env;
    env.rows = 1;
    env.cols = 10;
    env.num_of_agents = 1;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_timestep = 0;
    env.goal_locations.resize(env.num_of_agents);
    env.staged_actions.resize(env.num_of_agents);
    env.curr_states = {State(0, 0, 0)};
    env.curr_task_schedule.assign(env.num_of_agents, -1);
    env.new_freeagents = {0};
    env.new_tasks = {0, 1};

    env.task_pool.emplace(0, Task(0, std::list<int>{2, 3}, 0));
    env.task_pool.emplace(1, Task(1, std::list<int>{8, 9}, 0));
    return env;
}

SharedEnvironment build_multi_free_env()
{
    SharedEnvironment env;
    env.rows = 2;
    env.cols = 5;
    env.num_of_agents = 4;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_timestep = 0;
    env.goal_locations.resize(env.num_of_agents);
    env.staged_actions.resize(env.num_of_agents);
    env.curr_states = {
        State(0, 0, 0),
        State(1, 0, 0),
        State(5, 0, 0),
        State(6, 0, 0)
    };
    env.curr_task_schedule.assign(env.num_of_agents, -1);
    env.new_freeagents = {0, 1, 2, 3};
    env.new_tasks = {0, 1, 2, 3};

    env.task_pool.emplace(0, Task(0, std::list<int>{2, 3}, 0));
    env.task_pool.emplace(1, Task(1, std::list<int>{4, 9}, 0));
    env.task_pool.emplace(2, Task(2, std::list<int>{7, 8}, 0));
    env.task_pool.emplace(3, Task(3, std::list<int>{1, 6}, 0));
    return env;
}

SharedEnvironment build_hungarian_advantage_env()
{
    SharedEnvironment env;
    env.rows = 1;
    env.cols = 20;
    env.num_of_agents = 2;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_timestep = 0;
    env.goal_locations.resize(env.num_of_agents);
    env.staged_actions.resize(env.num_of_agents);
    env.curr_states = {
        State(0, 0, 0),
        State(8, 0, 0)
    };
    env.curr_task_schedule.assign(env.num_of_agents, -1);
    env.new_freeagents = {0, 1};
    env.new_tasks = {0, 1};

    env.task_pool.emplace(0, Task(0, std::list<int>{7}, 0));
    env.task_pool.emplace(1, Task(1, std::list<int>{1, 11}, 0));
    return env;
}

SharedEnvironment build_prestart_reassignment_env()
{
    SharedEnvironment env;
    env.rows = 1;
    env.cols = 10;
    env.num_of_agents = 2;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_timestep = 0;
    env.goal_locations.resize(env.num_of_agents);
    env.staged_actions.resize(env.num_of_agents);
    env.curr_states = {
        State(1, 0, 0),
        State(9, 0, 0)
    };
    env.curr_task_schedule = {-1, 0};
    env.new_freeagents = {0};
    env.new_tasks = {1};

    Task reserved_task(0, std::list<int>{2, 3}, 0);
    reserved_task.agent_assigned = 1;
    env.task_pool.emplace(0, reserved_task);
    env.task_pool.emplace(1, Task(1, std::list<int>{8, 7}, 0));
    return env;
}

SharedEnvironment build_started_task_env()
{
    SharedEnvironment env;
    env.rows = 1;
    env.cols = 10;
    env.num_of_agents = 2;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_timestep = 0;
    env.goal_locations.resize(env.num_of_agents);
    env.staged_actions.resize(env.num_of_agents);
    env.curr_states = {
        State(1, 0, 0),
        State(3, 0, 0)
    };
    env.curr_task_schedule = {-1, 0};
    env.new_freeagents = {0};
    env.new_tasks = {1};

    Task started_task(0, std::list<int>{2, 3}, 0);
    started_task.agent_assigned = 1;
    started_task.idx_next_loc = 1;
    env.task_pool.emplace(0, started_task);
    env.task_pool.emplace(1, Task(1, std::list<int>{0}, 0));
    return env;
}

struct PlanCaptureResult
{
    std::vector<int> proposed_schedule;
    std::string log;
};

PlanCaptureResult run_plan_and_capture(SharedEnvironment env, const UserTaskScheduler::TaskDifficultyScoringConfig& config)
{
    UserTaskScheduler::set_task_difficulty_scoring_config(config);
    UserTaskScheduler::initialize(1000, &env);

    PlanCaptureResult result;
    std::ostringstream capture;
    std::streambuf* original = std::cout.rdbuf(capture.rdbuf());
    UserTaskScheduler::plan(1000, result.proposed_schedule, &env);
    std::cout.rdbuf(original);
    result.log = capture.str();
    return result;
}

PlanCaptureResult rerun_plan_and_capture(
    SharedEnvironment env,
    const UserTaskScheduler::TaskDifficultyScoringConfig& config,
    std::vector<int> initial_proposed_schedule)
{
    UserTaskScheduler::set_task_difficulty_scoring_config(config);
    UserTaskScheduler::initialize(1000, &env);

    PlanCaptureResult result;
    result.proposed_schedule = std::move(initial_proposed_schedule);
    std::ostringstream capture;
    std::streambuf* original = std::cout.rdbuf(capture.rdbuf());
    UserTaskScheduler::plan(1000, result.proposed_schedule, &env);
    std::cout.rdbuf(original);
    result.log = capture.str();
    return result;
}

} // namespace

int main()
{
    std::cout << "case1" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        const PlanCaptureResult result = run_plan_and_capture(build_nearest_task_env(), config);
        assert(result.proposed_schedule.size() == 1);
        assert(result.proposed_schedule[0] == 0);
    }

    std::cout << "case2" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        config.max_assignments_per_plan = 2;
        const PlanCaptureResult result = run_plan_and_capture(build_multi_free_env(), config);
        const int assigned = static_cast<int>(std::count_if(
            result.proposed_schedule.begin(),
            result.proposed_schedule.end(),
            [](int task_id) { return task_id != -1; }));
        assert(assigned == 2);
    }

    std::cout << "case3" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        config.debug_log = true;
        config.local_task_filter_enabled = true;
        config.local_task_bucket_size = 4;
        config.local_task_min_candidates = 1;
        config.local_task_max_bucket_radius = 0;
        const PlanCaptureResult result = run_plan_and_capture(build_multi_free_env(), config);
        assert(result.log.find("completion_evals=") != std::string::npos);
    }

    std::cout << "case4" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig greedy_config;
        const PlanCaptureResult greedy_result = run_plan_and_capture(build_hungarian_advantage_env(), greedy_config);
        assert(greedy_result.proposed_schedule.size() == 2);
        assert(greedy_result.proposed_schedule[0] == 0);
        assert(greedy_result.proposed_schedule[1] == 1);

        UserTaskScheduler::TaskDifficultyScoringConfig hungarian_config;
        hungarian_config.debug_log = true;
        hungarian_config.hungarian_matching_enabled = true;
        const PlanCaptureResult hungarian_result = run_plan_and_capture(build_hungarian_advantage_env(), hungarian_config);
        assert(hungarian_result.proposed_schedule.size() == 2);
        assert(hungarian_result.proposed_schedule[0] == 1);
        assert(hungarian_result.proposed_schedule[1] == 0);
        assert(hungarian_result.log.find("(hungarian") != std::string::npos);
    }

    std::cout << "case5" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        config.hungarian_matching_enabled = true;
        config.prestart_reassignment_enabled = true;
        const PlanCaptureResult result = run_plan_and_capture(build_prestart_reassignment_env(), config);
        assert(result.proposed_schedule.size() == 2);
        assert(result.proposed_schedule[0] == 0);
        assert(result.proposed_schedule[1] == -1);
    }

    std::cout << "case6" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        config.hungarian_matching_enabled = true;
        config.prestart_reassignment_enabled = true;
        const PlanCaptureResult result = run_plan_and_capture(build_started_task_env(), config);
        assert(result.proposed_schedule.size() == 2);
        assert(result.proposed_schedule[0] == 1);
        assert(result.proposed_schedule[1] == 0);
    }

    std::cout << "case7" << std::endl;
    {
        UserTaskScheduler::TaskDifficultyScoringConfig config;
        config.hungarian_matching_enabled = true;
        config.prestart_reassignment_enabled = true;

        SharedEnvironment env = build_hungarian_advantage_env();
        const std::vector<int> in_flight_schedule = {0, 1};
        const PlanCaptureResult result = rerun_plan_and_capture(env, config, in_flight_schedule);
        assert(result.proposed_schedule.size() == 2);
        assert(result.proposed_schedule == in_flight_schedule);
    }

    std::cout << "task_difficulty_scoring test passed" << std::endl;
    return 0;
}
