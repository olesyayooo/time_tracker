#pragma once

#include <json/json.h>
#include <string>

namespace time_tracker {
class TaskRepository {
public:
    Json::Value create(const std::string& owner, const std::string& project_id, const std::string& title) const;
    Json::Value list(const std::string& owner, const std::string& project_id) const;
    Json::Value get(const std::string& owner, const std::string& id) const;
    Json::Value update(const std::string& owner, const std::string& id, const Json::Value& patch) const;
    void remove(const std::string& owner, const std::string& id) const;
};
}
