#pragma once

#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <functional>

namespace time_tracker {
struct EntryFilter {
    std::string from, to, project_id, task_id;
    int limit=100, offset=0;
};
class TimeEntryRepository {
public:
    using Db=drogon::orm::DbClientPtr;
    // Сериализация операций одного пользователя и подтверждённый COMMIT перед HTTP-ответом.
    Json::Value withUserLock(const std::string& owner, const std::function<Json::Value(const Db&)>& action) const;
    Json::Value task(const Db& db, const std::string& owner, const std::string& id) const;
    Json::Value active(const Db& db, const std::string& owner) const;
    Json::Value entry(const Db& db, const std::string& owner, const std::string& id) const;
    std::string now(const Db& db) const;
    bool overlaps(const Db& db, const std::string& owner, const std::string& start,
                  const std::string& end, const std::string& except="") const;
    Json::Value insert(const Db& db, const std::string& owner, const std::string& task_id,
                       const std::string& start, const std::string& end) const;
    Json::Value update(const Db& db, const std::string& owner, const std::string& id,
                       const std::string& start, const std::string& end) const;
    void remove(const Db& db, const std::string& owner, const std::string& id) const;
    Json::Value paused(const Db& db, const std::string& id, bool paused) const;
    Json::Value list(const std::string& owner, const EntryFilter& filter) const;
};
}
