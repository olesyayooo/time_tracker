#pragma once

#include "repositories/TaskRepository.h"
#include "repositories/ProjectRepository.h"

namespace time_tracker {
class TaskService {
public:
    Json::Value create(const std::string& owner, const std::string& project_id, const Json::Value& body) const;
    Json::Value list(const std::string& owner, const std::string& project_id) const;
    Json::Value get(const std::string& owner, const std::string& id) const;
    Json::Value update(const std::string& owner, const std::string& id, Json::Value body) const;
    void remove(const std::string& owner, const std::string& id) const;
private:
    static void checkId(const std::string& id);
    TaskRepository tasks_;
    ProjectRepository projects_;
};
}
