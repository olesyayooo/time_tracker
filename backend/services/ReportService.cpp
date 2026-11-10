#include "ReportService.h"
#include "TimeFilters.h"

namespace time_tracker {
Json::Value ReportService::report(const std::string& owner,const Json::Value& query) const {
    allowedFields(query,{"from","to","group_by","project_id","task_id"});
    auto filter=scopeFilters(owner,query);
    filter.from=textField(query,"from"); filter.to=textField(query,"to");
    validDate(filter.from); validDate(filter.to);
    if (filter.from>filter.to) throw ApiError(drogon::k400BadRequest,"from must not be after to");
    const auto group=query.isMember("group_by") ? textField(query,"group_by") : "project";
    if (group!="project" && group!="task" && group!="day")
        throw ApiError(drogon::k400BadRequest,"group_by must be project, task or day");
    return reports_.report(owner,filter,group);
}
}
