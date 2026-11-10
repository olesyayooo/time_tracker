#include "ProjectService.h"
#include "Validation.h"
namespace time_tracker {
void ProjectService::checkId(const std::string& id) {
    if (!validUuid(id)) throw ApiError(drogon::k400BadRequest,"Invalid project UUID");
}
Json::Value ProjectService::create(const std::string& owner,const Json::Value& body) const {
    allowedFields(body,{"name"});
    return projects_.create(owner,validName(textField(body,"name"),150));
}
Json::Value ProjectService::get(const std::string& owner,const std::string& id) const {
    checkId(id); return projects_.get(owner,id);
}
Json::Value ProjectService::update(const std::string& owner,const std::string& id,Json::Value body) const {
    checkId(id); allowedFields(body,{"name","is_completed"});
    if (body.empty()) throw ApiError(drogon::k400BadRequest,"Patch must not be empty");
    if (body.isMember("name")) body["name"]=validName(textField(body,"name"),150);
    if (body.isMember("is_completed") && !body["is_completed"].isBool())
        throw ApiError(drogon::k400BadRequest,"is_completed must be boolean");
    return projects_.update(owner,id,body);
}
void ProjectService::remove(const std::string& owner,const std::string& id) const {
    checkId(id);
    projects_.remove(owner,id);
}
}
