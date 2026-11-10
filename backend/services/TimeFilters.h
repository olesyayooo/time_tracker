#pragma once
#include "TimeValidation.h"
#include "repositories/ProjectRepository.h"
#include "repositories/TaskRepository.h"
#include "repositories/TimeEntryRepository.h"

namespace time_tracker {
inline EntryFilter scopeFilters(const std::string& owner,const Json::Value& query) {
    EntryFilter filter;
    if (query.isMember("project_id")) {
        filter.project_id=textField(query,"project_id"); checkUuid(filter.project_id);
        ProjectRepository{}.get(owner,filter.project_id);
    }
    if (query.isMember("task_id")) {
        filter.task_id=textField(query,"task_id"); checkUuid(filter.task_id);
        const auto task=TaskRepository{}.get(owner,filter.task_id);
        // UUID может быть передан с буквами в верхнем регистре.
        if (!filter.project_id.empty()) {
            auto project=ProjectRepository{}.get(owner,filter.project_id);
            if (task["project_id"].asString()!=project["id"].asString())
                throw ApiError(drogon::k400BadRequest,"Task does not belong to the selected project");
        }
    }
    return filter;
}
}
