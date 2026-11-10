#include "TaskService.h"
#include "Validation.h"

namespace time_tracker {
void TaskService::checkId(const std::string& id) {
    if (!validUuid(id)) throw ApiError(drogon::k400BadRequest, "Invalid UUID");
}
Json::Value TaskService::create(const std::string& owner, const std::string& project_id,
                                const Json::Value& body) const {
    checkId(project_id);
    allowedFields(body, {"title"});
    return tasks_.create(owner, project_id, validName(textField(body, "title"), 200));
}
Json::Value TaskService::list(const std::string& owner, const std::string& project_id) const {
    checkId(project_id);
    projects_.get(owner, project_id); // Чужой/несуществующий проект — 404, а не пустой список.
    return tasks_.list(owner, project_id);
}
Json::Value TaskService::get(const std::string& owner, const std::string& id) const {
    checkId(id);
    return tasks_.get(owner, id);
}
Json::Value TaskService::update(const std::string& owner, const std::string& id, Json::Value body) const {
    checkId(id);
    allowedFields(body, {"title", "is_completed"});
    if (body.empty()) throw ApiError(drogon::k400BadRequest, "Patch must not be empty");
    if (body.isMember("title")) body["title"] = validName(textField(body, "title"), 200);
    if (body.isMember("is_completed") && !body["is_completed"].isBool())
        throw ApiError(drogon::k400BadRequest, "is_completed must be boolean");
    return tasks_.update(owner, id, body);
}
void TaskService::remove(const std::string& owner, const std::string& id) const {
    checkId(id);
    tasks_.remove(owner, id);
}
}
