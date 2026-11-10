#pragma once
#include "models/User.h"
#include <drogon/orm/DbClient.h>
#include <optional>
namespace time_tracker {
class UserRepository {
public:
    std::optional<User> create(const std::string& name, const std::string& email, const std::string& hash) const;
    std::optional<User> byEmail(const std::string& email) const;
    std::optional<User> byId(const std::string& id) const;
};
}
