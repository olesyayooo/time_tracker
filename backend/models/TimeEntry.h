#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace time_tracker {

struct TimeEntry {
    std::string id;
    std::string user_id;
    std::string task_id;
    std::chrono::system_clock::time_point started_at{};
    // nullopt соответствует SQL NULL: интервал ещё активен.
    std::optional<std::chrono::system_clock::time_point> ended_at;
};

} // namespace time_tracker
