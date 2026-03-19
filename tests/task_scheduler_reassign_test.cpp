#include <cassert>
#include <iostream>
#include "TaskScheduler.h"
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

    Task task_a;
    task_a.task_id = 100;
    task_a.t_revealed = 0;
    task_a.agent_assigned = 0;
    task_a.locations = {1, 2};

    Task task_b;
    task_b.task_id = 101;
    task_b.t_revealed = 0;
    task_b.locations = {10, 9};

    env.task_pool[task_a.task_id] = task_a;
    env.task_pool[task_b.task_id] = task_b;

    auto* scheduler = new TaskScheduler(&env);
    scheduler->initialize(1000);

    std::vector<int> proposed_schedule;
    scheduler->plan(100, proposed_schedule);

    assert(proposed_schedule.size() == 2);
    assert(proposed_schedule[0] == 100);
    assert(proposed_schedule[1] == 101);

    scheduler->env = nullptr;
    delete scheduler;

    std::cout << "task_scheduler_reassign_test passed" << std::endl;
    return 0;
}
