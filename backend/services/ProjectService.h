#pragma once
#include "repositories/ProjectRepository.h"
namespace time_tracker {
class ProjectService {
public:
    Json::Value create(const std::string& owner,const Json::Value& body) const;
    Json::Value list(const std::string& owner) const { return projects_.list(owner); }
    Json::Value get(const std::string& owner,const std::string& id) const;
    Json::Value update(const std::string& owner,const std::string& id,Json::Value body) const;
    void remove(const std::string& owner,const std::string& id) const;
private:
    static void checkId(const std::string& id);
    ProjectRepository projects_;
};
}
