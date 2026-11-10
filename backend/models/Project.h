#pragma once

#include <chrono>
#include <string>

namespace time_tracker {

struct Project {
    std::string id;
    std::string user_id;
    std::string name;
    bool is_completed = false;
    std::chrono::system_clock::time_point created_at{};
};

} // namespace time_tracker
