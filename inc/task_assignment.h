#ifndef TASK_ASSIGNMENT_H
#define TASK_ASSIGNMENT_H

#include "SharedEnv.h"

#include <utility>
#include <vector>

namespace TaskAssignment {

void initialize_task_scoring(const SharedEnvironment* env);

int estimate_task_cost(const SharedEnvironment* env, int agent_id, int task_id);

std::vector<std::pair<int, int>> solve_bipartite_matching(
    const SharedEnvironment* env,
    const std::vector<int>& free_agents,
    const std::vector<int>& free_tasks);

}

#endif
