#ifndef PLANNER
#define PLANNER

#include "Types.h"
#include "TrajLNS.h"
#include <random>
#include <vector>


namespace DefaultPlanner{

    
    void initialize(int preprocess_time_limit, SharedEnvironment* env);

    void plan(int time_limit, std::vector<std::vector<Action>> & actions,
                        SharedEnvironment* env, int num_steps=10);

    const std::vector<int>& get_task_route_density_snapshot();

}
#endif