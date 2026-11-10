#pragma once

#include <chrono>
#include <string>

namespace time_tracker {

struct Task {
    std::string id;
    std::string project_id;
    std::string title;
    bool is_completed = false;
    bool is_paused = false; // Меняется операциями таймера, а не CRUD задачи.
    std::chrono::system_clock::time_point created_at{};
};

} // namespace time_tracker
