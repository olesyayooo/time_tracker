#pragma once

#include <string>

namespace time_tracker {

struct User {
    std::string id; // UUID в текстовом представлении.
    std::string name;
    std::string email;
    std::string password_hash; // Только для серверной логики, не для JSON-ответа.
};

} // namespace time_tracker
