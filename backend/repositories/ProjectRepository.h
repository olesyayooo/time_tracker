#pragma once
#include <drogon/orm/DbClient.h>
#include <json/json.h>
namespace time_tracker {
class ProjectRepository {
public:
    Json::Value create(const std::string& owner,const std::string& name) const;
    Json::Value list(const std::string& owner) const;
    Json::Value get(const std::string& owner,const std::string& id) const;
    Json::Value update(const std::string& owner,const std::string& id,const Json::Value& patch) const;
    void remove(const std::string& owner,const std::string& id) const;
};
}
