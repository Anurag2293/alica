#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace alica_sim_test
{

struct TestIO
{
    double x = 0;
    double vx = 0;
    std::vector<std::pair<std::string, int64_t>> events;
    std::function<void(const std::string&)> onEvent;
};

} // namespace alica_sim_test
