#include <cassert>
#include <iostream>
#include <vector>

#include "scheduler.h"
#include "Tasks.h"

int main()
{
    SharedEnvironment env;
    env.rows = 3;
    env.cols = 4;
    env.num_of_agents = 2;
    env.map.assign(env.rows * env.cols, 0);
    env.curr_states = {
        State(0, 0, 0),
        State(11, 0, 2)
    };
    env.curr_task_schedule = {100, -1};
    env.goal_locations.resize(env.num_of_agents);
    env.goal_locations[0].push_back({1, 0});

    Task held_task;
    held_task.task_id = 100;
    held_task.t_revealed = 0;
    held_task.agent_assigned = 0;
    held_task.locations = {1, 2};

    Task free_task;
    free_task.task_id = 101;
    free_task.t_revealed = 0;
    free_task.locations = {10, 9};

    env.task_pool[held_task.task_id] = held_task;
    env.task_pool[free_task.task_id] = free_task;

    DefaultPlanner::schedule_initialize(1000, &env);

    std::vector<int> proposed_schedule;
    DefaultPlanner::schedule_plan(100, proposed_schedule, &env);

    assert(proposed_schedule.size() == 2);
    assert(proposed_schedule[0] == 100);
    assert(proposed_schedule[1] == 101);

    std::cout << "scheduler_reassign_test passed" << std::endl;
    return 0;
}
