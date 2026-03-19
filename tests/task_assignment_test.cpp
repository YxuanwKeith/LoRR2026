#include <cassert>
#include <iostream>
#include <vector>

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
    env.curr_task_schedule = {-1, -1};

    Task left_task;
    left_task.task_id = 100;
    left_task.t_revealed = 0;
    left_task.locations = {1, 2};

    Task right_task;
    right_task.task_id = 101;
    right_task.t_revealed = 0;
    right_task.locations = {10, 9};

    env.task_pool[left_task.task_id] = left_task;
    env.task_pool[right_task.task_id] = right_task;
    auto* scheduler = new TaskScheduler(&env);
    scheduler->initialize(1000);

    std::vector<int> proposed_schedule(env.num_of_agents, -1);
    scheduler->plan(100, proposed_schedule);

    assert(proposed_schedule[0] == 100);
    assert(proposed_schedule[1] == 101);

    scheduler->env = nullptr;
    delete scheduler;

    std::cout << "task_assignment_test passed" << std::endl;
    return 0;
}
