#include <cassert>
#include <iostream>
#include <string>

#include "RuntimeOptions.h"

int main()
{
    RuntimeOptions::clear();

    std::string error;
    assert(RuntimeOptions::set_override_entry("taskDifficultyScoring.enabled=true", &error));
    assert(RuntimeOptions::set_override_entry("taskDifficultyScoring.corridorSlack=2", &error));
    assert(RuntimeOptions::set_override_entry("taskDifficultyScoring.congestionWeight=4.5", &error));

    bool enabled = false;
    int corridor_slack = 0;
    double congestion_weight = 0.0;

    assert(RuntimeOptions::get_bool("taskDifficultyScoring.enabled", enabled) && enabled);
    assert(RuntimeOptions::get_int("taskDifficultyScoring.corridorSlack", corridor_slack) && corridor_slack == 2);
    assert(RuntimeOptions::get_double("taskDifficultyScoring.congestionWeight", congestion_weight) && congestion_weight == 4.5);

    assert(!RuntimeOptions::set_override_entry("invalid_entry", &error));
    assert(!error.empty());

    const auto entries = RuntimeOptions::list_override_entries();
    assert(entries.size() == 3);

    RuntimeOptions::clear();
    assert(!RuntimeOptions::has_override("taskDifficultyScoring.enabled"));

    std::cout << "runtime_options test passed" << std::endl;
    return 0;
}
