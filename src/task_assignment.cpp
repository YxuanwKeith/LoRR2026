#include "task_assignment.h"

#include "Tasks.h"
#include "heuristics.h"
#include "planner.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace TaskAssignment {
namespace {

constexpr int kLargeAssignmentCost = 100000000;
constexpr double kMinTaskWeight = 0.25;
constexpr double kMaxTaskWeight = 3.0;
constexpr double kNeighborBypassScale = 0.5;

struct TaskScoringConfig
{
    bool enabled = false;
    double length_bonus = 0.45;
    double bypass_bonus = 0.35;
    double density_penalty = 0.80;
    int live_density_radius = 2;
    int route_density_radius = 2;
    int route_density_cap = 24;
};

struct StaticTaskScoringCache
{
    bool ready = false;
    int rows = -1;
    int cols = -1;
    size_t map_size = 0;
    std::vector<double> cell_bypass_bonus;
    double max_cell_bypass_bonus = 1.0;
};

struct TaskContext
{
    double bypass_bonus = 0.0;
    double density_penalty = 0.0;
};

struct EpisodeTaskScoringCache
{
    int timestep = -1;
    size_t task_pool_size = 0;
    std::unordered_map<int, TaskContext> task_contexts;
};

TaskScoringConfig g_task_scoring_config;
bool g_task_scoring_config_loaded = false;
StaticTaskScoringCache g_static_cache;
EpisodeTaskScoringCache g_episode_cache;

bool parse_env_flag(const char* name, bool default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        return default_value;
    }
    const std::string flag(value);
    return flag == "1" || flag == "true" || flag == "TRUE" || flag == "on" || flag == "ON";
}

double parse_env_double(const char* name, double default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        return default_value;
    }
    try
    {
        return std::stod(value);
    }
    catch (...)
    {
        return default_value;
    }
}

int parse_env_int(const char* name, int default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        return default_value;
    }
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return default_value;
    }
}

const TaskScoringConfig& get_task_scoring_config()
{
    if (!g_task_scoring_config_loaded)
    {
        g_task_scoring_config.enabled = parse_env_flag("LORR_ENABLE_TASK_SCORING", false);
        g_task_scoring_config.length_bonus = parse_env_double("LORR_TASK_SCORE_LENGTH_BONUS", 0.45);
        g_task_scoring_config.bypass_bonus = parse_env_double("LORR_TASK_SCORE_BYPASS_BONUS", 0.35);
        g_task_scoring_config.density_penalty = parse_env_double("LORR_TASK_SCORE_DENSITY_PENALTY", 0.80);
        g_task_scoring_config.live_density_radius = std::max(0, parse_env_int("LORR_TASK_SCORE_LIVE_RADIUS", 2));
        g_task_scoring_config.route_density_radius = std::max(0, parse_env_int("LORR_TASK_SCORE_ROUTE_RADIUS", 2));
        g_task_scoring_config.route_density_cap = std::max(1, parse_env_int("LORR_TASK_SCORE_ROUTE_CAP", 24));
        g_task_scoring_config_loaded = true;
    }
    return g_task_scoring_config;
}

double clamp_double(double value, double lower, double upper)
{
    return std::max(lower, std::min(upper, value));
}

bool static_cache_matches_env(const SharedEnvironment* env)
{
    return g_static_cache.ready &&
           g_static_cache.rows == env->rows &&
           g_static_cache.cols == env->cols &&
           g_static_cache.map_size == env->map.size();
}

void reset_episode_cache(const SharedEnvironment* env)
{
    g_episode_cache.timestep = env == nullptr ? -1 : env->curr_timestep;
    g_episode_cache.task_pool_size = env == nullptr ? 0 : env->task_pool.size();
    g_episode_cache.task_contexts.clear();
}

void ensure_episode_cache(const SharedEnvironment* env)
{
    if (env == nullptr)
    {
        reset_episode_cache(nullptr);
        return;
    }
    if (g_episode_cache.timestep != env->curr_timestep ||
        g_episode_cache.task_pool_size != env->task_pool.size())
    {
        reset_episode_cache(env);
    }
}

void ensure_static_cache(const SharedEnvironment* env)
{
    if (env == nullptr || static_cache_matches_env(env))
    {
        return;
    }

    DefaultPlanner::init_heuristics(const_cast<SharedEnvironment*>(env));
    const auto& neighbors = DefaultPlanner::global_neighbors;

    g_static_cache.ready = true;
    g_static_cache.rows = env->rows;
    g_static_cache.cols = env->cols;
    g_static_cache.map_size = env->map.size();
    g_static_cache.cell_bypass_bonus.assign(env->map.size(), 0.0);
    g_static_cache.max_cell_bypass_bonus = 1.0;

    for (int loc = 0; loc < static_cast<int>(env->map.size()); ++loc)
    {
        if (env->map[loc] != 0)
        {
            continue;
        }

        const auto& cell_neighbors = neighbors[loc];
        const double local_branch = std::max(0, static_cast<int>(cell_neighbors.size()) - 1);
        double neighbor_branch = 0.0;
        for (int next_loc : cell_neighbors)
        {
            neighbor_branch += std::max(0, static_cast<int>(neighbors[next_loc].size()) - 2);
        }
        if (!cell_neighbors.empty())
        {
            neighbor_branch /= static_cast<double>(cell_neighbors.size());
        }

        const double bypass_bonus = local_branch + kNeighborBypassScale * neighbor_branch;
        g_static_cache.cell_bypass_bonus[loc] = bypass_bonus;
        g_static_cache.max_cell_bypass_bonus = std::max(g_static_cache.max_cell_bypass_bonus, bypass_bonus);
    }
}

int remaining_task_chain_cost(const SharedEnvironment* env, const Task& task)
{
    if (task.idx_next_loc >= static_cast<int>(task.locations.size()))
    {
        return 0;
    }

    int cost = 0;
    for (int idx = task.idx_next_loc; idx + 1 < static_cast<int>(task.locations.size()); ++idx)
    {
        cost += DefaultPlanner::get_h(const_cast<SharedEnvironment*>(env), task.locations[idx], task.locations[idx + 1]);
    }
    return cost;
}

double average_task_bypass_bonus(const SharedEnvironment* env, const Task& task)
{
    ensure_static_cache(env);
    if (!g_static_cache.ready || task.idx_next_loc >= static_cast<int>(task.locations.size()))
    {
        return 0.0;
    }

    double total_bonus = 0.0;
    int counted = 0;
    for (int idx = task.idx_next_loc; idx < static_cast<int>(task.locations.size()); ++idx)
    {
        const int loc = task.locations[idx];
        if (loc < 0 || loc >= static_cast<int>(g_static_cache.cell_bypass_bonus.size()))
        {
            continue;
        }
        total_bonus += g_static_cache.cell_bypass_bonus[loc];
        counted++;
    }

    if (counted == 0)
    {
        return 0.0;
    }
    return clamp_double(total_bonus / (counted * g_static_cache.max_cell_bypass_bonus), 0.0, 1.0);
}

double compute_live_density(const SharedEnvironment* env, int center_loc, int radius)
{
    if (env == nullptr || radius < 0 || env->num_of_agents <= 0)
    {
        return 0.0;
    }

    const int center_row = center_loc / env->cols;
    const int center_col = center_loc % env->cols;
    int count = 0;
    for (const auto& state : env->curr_states)
    {
        const int row = state.location / env->cols;
        const int col = state.location % env->cols;
        const int manhattan = std::abs(row - center_row) + std::abs(col - center_col);
        if (manhattan <= radius)
        {
            count++;
        }
    }

    return clamp_double(static_cast<double>(count) / std::max(1, env->num_of_agents), 0.0, 1.0);
}

double compute_route_density(const SharedEnvironment* env,
                             const std::vector<int>& route_density_snapshot,
                             int center_loc,
                             int radius,
                             int route_density_cap)
{
    if (env == nullptr || route_density_snapshot.empty() || radius < 0)
    {
        return 0.0;
    }

    const int center_row = center_loc / env->cols;
    const int center_col = center_loc % env->cols;
    int density_sum = 0;
    for (int row = std::max(0, center_row - radius); row <= std::min(env->rows - 1, center_row + radius); ++row)
    {
        for (int col = std::max(0, center_col - radius); col <= std::min(env->cols - 1, center_col + radius); ++col)
        {
            if (std::abs(row - center_row) + std::abs(col - center_col) > radius)
            {
                continue;
            }
            const int loc = row * env->cols + col;
            density_sum += route_density_snapshot[loc];
        }
    }

    return clamp_double(static_cast<double>(density_sum) / std::max(1, route_density_cap), 0.0, 1.0);
}

TaskContext build_task_context(const SharedEnvironment* env, const Task& task)
{
    TaskContext context;
    if (env == nullptr || task.idx_next_loc >= static_cast<int>(task.locations.size()))
    {
        return context;
    }

    const auto& cfg = get_task_scoring_config();
    const auto& route_density_snapshot = DefaultPlanner::get_task_route_density_snapshot();

    context.bypass_bonus = average_task_bypass_bonus(env, task);

    double total_density = 0.0;
    int counted = 0;
    for (int idx = task.idx_next_loc; idx < static_cast<int>(task.locations.size()); ++idx)
    {
        const int loc = task.locations[idx];
        const double live_density = compute_live_density(env, loc, cfg.live_density_radius);
        const double planned_density = compute_route_density(env, route_density_snapshot, loc, cfg.route_density_radius, cfg.route_density_cap);
        total_density += 0.5 * live_density + 0.5 * planned_density;
        counted++;
    }

    if (counted > 0)
    {
        context.density_penalty = clamp_double(total_density / counted, 0.0, 1.0);
    }
    return context;
}

const TaskContext& get_task_context(const SharedEnvironment* env, const Task& task)
{
    ensure_episode_cache(env);
    auto it = g_episode_cache.task_contexts.find(task.task_id);
    if (it != g_episode_cache.task_contexts.end())
    {
        return it->second;
    }

    auto [inserted_it, inserted] = g_episode_cache.task_contexts.emplace(task.task_id, build_task_context(env, task));
    (void) inserted;
    return inserted_it->second;
}

double evaluate_task_weight(const SharedEnvironment* env, const Task& task, int total_length_cost)
{
    const auto& cfg = get_task_scoring_config();
    if (!cfg.enabled)
    {
        return 1.0;
    }

    const TaskContext& context = get_task_context(env, task);
    const double map_scale = std::max(1, env->rows + env->cols);
    const double length_bonus = map_scale / (map_scale + std::max(0, total_length_cost));

    const double score = 1.0
        + cfg.length_bonus * length_bonus
        + cfg.bypass_bonus * context.bypass_bonus
        - cfg.density_penalty * context.density_penalty;

    return clamp_double(score, kMinTaskWeight, kMaxTaskWeight);
}

int apply_task_weight(int base_cost, double weight)
{
    if (base_cost >= kLargeAssignmentCost)
    {
        return kLargeAssignmentCost;
    }

    const double safe_weight = clamp_double(weight, kMinTaskWeight, kMaxTaskWeight);
    const long long weighted_cost = std::llround(static_cast<double>(base_cost) / safe_weight);
    if (weighted_cost <= 0)
    {
        return 1;
    }
    if (weighted_cost >= kLargeAssignmentCost)
    {
        return kLargeAssignmentCost - 1;
    }
    return static_cast<int>(weighted_cost);
}

std::vector<int> hungarian_min_cost(const std::vector<std::vector<int>>& cost)
{
    const int rows = static_cast<int>(cost.size());
    const int cols = rows == 0 ? 0 : static_cast<int>(cost.front().size());
    if (rows == 0 || cols == 0)
    {
        return {};
    }
    if (rows > cols)
    {
        throw std::invalid_argument("hungarian_min_cost requires rows <= cols");
    }

    const long long inf = std::numeric_limits<long long>::max() / 4;
    std::vector<long long> u(rows + 1, 0), v(cols + 1, 0), minv(cols + 1, 0);
    std::vector<int> p(cols + 1, 0), way(cols + 1, 0);

    for (int i = 1; i <= rows; ++i)
    {
        p[0] = i;
        std::fill(minv.begin(), minv.end(), inf);
        std::vector<bool> used(cols + 1, false);
        int j0 = 0;
        do
        {
            used[j0] = true;
            const int i0 = p[j0];
            long long delta = inf;
            int j1 = 0;
            for (int j = 1; j <= cols; ++j)
            {
                if (used[j])
                {
                    continue;
                }
                const long long cur = static_cast<long long>(cost[i0 - 1][j - 1]) - u[i0] - v[j];
                if (cur < minv[j])
                {
                    minv[j] = cur;
                    way[j] = j0;
                }
                if (minv[j] < delta)
                {
                    delta = minv[j];
                    j1 = j;
                }
            }
            for (int j = 0; j <= cols; ++j)
            {
                if (used[j])
                {
                    u[p[j]] += delta;
                    v[j] -= delta;
                }
                else
                {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        do
        {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    std::vector<int> row_to_col(rows, -1);
    for (int j = 1; j <= cols; ++j)
    {
        if (p[j] != 0)
        {
            row_to_col[p[j] - 1] = j - 1;
        }
    }
    return row_to_col;
}

std::vector<std::pair<int, int>> solve_rectangular_assignment(
    const SharedEnvironment* env,
    const std::vector<int>& agents,
    const std::vector<int>& tasks)
{
    std::vector<std::pair<int, int>> assignments;
    if (agents.empty() || tasks.empty())
    {
        return assignments;
    }

    if (agents.size() <= tasks.size())
    {
        std::vector<std::vector<int>> cost(agents.size(), std::vector<int>(tasks.size(), kLargeAssignmentCost));
        for (size_t row = 0; row < agents.size(); ++row)
        {
            for (size_t col = 0; col < tasks.size(); ++col)
            {
                cost[row][col] = estimate_task_cost(env, agents[row], tasks[col]);
            }
        }

        const std::vector<int> row_to_col = hungarian_min_cost(cost);
        for (size_t row = 0; row < row_to_col.size(); ++row)
        {
            const int col = row_to_col[row];
            if (col >= 0 && cost[row][col] < kLargeAssignmentCost)
            {
                assignments.emplace_back(agents[row], tasks[col]);
            }
        }
        return assignments;
    }

    std::vector<std::vector<int>> transposed(tasks.size(), std::vector<int>(agents.size(), kLargeAssignmentCost));
    for (size_t row = 0; row < tasks.size(); ++row)
    {
        for (size_t col = 0; col < agents.size(); ++col)
        {
            transposed[row][col] = estimate_task_cost(env, agents[col], tasks[row]);
        }
    }

    const std::vector<int> row_to_col = hungarian_min_cost(transposed);
    for (size_t row = 0; row < row_to_col.size(); ++row)
    {
        const int col = row_to_col[row];
        if (col >= 0 && transposed[row][col] < kLargeAssignmentCost)
        {
            assignments.emplace_back(agents[col], tasks[row]);
        }
    }
    return assignments;
}

} // namespace

void initialize_task_scoring(const SharedEnvironment* env)
{
    ensure_static_cache(env);
    reset_episode_cache(env);
    (void) get_task_scoring_config();
}

int estimate_task_cost(const SharedEnvironment* env, int agent_id, int task_id)
{
    if (env == nullptr || agent_id < 0 || agent_id >= env->num_of_agents)
    {
        return kLargeAssignmentCost;
    }

    const auto task_it = env->task_pool.find(task_id);
    if (task_it == env->task_pool.end())
    {
        return kLargeAssignmentCost;
    }

    const Task& task = task_it->second;
    if (task.idx_next_loc >= static_cast<int>(task.locations.size()))
    {
        return kLargeAssignmentCost;
    }
    if (task.agent_assigned != -1 && task.agent_assigned != agent_id)
    {
        return kLargeAssignmentCost;
    }

    const int curr_loc = env->curr_states[agent_id].location;
    const int next_task_loc = task.locations[task.idx_next_loc];
    const int approach_cost = DefaultPlanner::get_h(const_cast<SharedEnvironment*>(env), curr_loc, next_task_loc);
    if (approach_cost >= MAX_TIMESTEP)
    {
        return kLargeAssignmentCost;
    }

    const int chain_cost = remaining_task_chain_cost(env, task);
    const int base_cost = approach_cost + chain_cost;
    const double weight = evaluate_task_weight(env, task, base_cost);
    return apply_task_weight(base_cost, weight);
}

std::vector<std::pair<int, int>> solve_bipartite_matching(
    const SharedEnvironment* env,
    const std::vector<int>& free_agents,
    const std::vector<int>& free_tasks)
{
    std::vector<int> agents = free_agents;
    std::vector<int> tasks = free_tasks;
    std::sort(agents.begin(), agents.end());
    std::sort(tasks.begin(), tasks.end());
    return solve_rectangular_assignment(env, agents, tasks);
}

} // namespace TaskAssignment
