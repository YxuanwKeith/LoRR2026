#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "Executor.h"

int main()
{
    auto env = std::make_unique<SharedEnvironment>();
    env->rows = 1;
    env->cols = 3;
    env->num_of_agents = 2;
    env->map.assign(env->rows * env->cols, 0);
    env->system_timestep = 0;
    env->curr_states = {
        State(0, 0, 0),
        State(2, 0, 2)
    };
    env->system_states = env->curr_states;
    env->staged_actions.resize(env->num_of_agents);
    env->min_planner_communication_time = 100;
    env->action_time = 1;
    env->max_counter = 1;

    Executor executor(env.release());
    executor.initialize(1000);

    Plan plan;
    plan.actions = {
        {Action::FW},
        {Action::FW}
    };

    executor.process_new_plan(100, plan, executor.env->staged_actions);

    std::vector<ExecutionCommand> commands(executor.env->num_of_agents, ExecutionCommand::STOP);
    executor.next_command(10, commands);

    assert(commands[0] == ExecutionCommand::GO);
    assert(commands[1] == ExecutionCommand::STOP);

    std::cout << "executor_tpg_test passed" << std::endl;
    return 0;
}
