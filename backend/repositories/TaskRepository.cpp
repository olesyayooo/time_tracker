#include "TaskRepository.h"
#include "services/ApiError.h"
#include <drogon/drogon.h>

namespace time_tracker {
namespace {
const std::string columns = "t.id,t.project_id,t.title,t.is_completed,t.is_paused,"
    "to_char(t.created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS created_at";

Json::Value task(const drogon::orm::Row& row) {
    Json::Value result;
    for (const auto* key : {"id", "project_id", "title", "created_at"})
        result[key] = row[key].as<std::string>();
    result["is_completed"] = row["is_completed"].as<bool>();
    result["is_paused"] = row["is_paused"].as<bool>();
    return result;
}
Json::Value one(const drogon::orm::Result& rows) {
    if (rows.empty()) throw ApiError(drogon::k404NotFound, "Task not found");
    return task(rows[0]);
}
}

Json::Value TaskRepository::create(const std::string& owner, const std::string& project_id,
                                   const std::string& title) const {
    auto transaction = drogon::app().getDbClient()->newTransaction();
    auto parent = transaction->execSqlSync(
        "SELECT id FROM projects WHERE user_id=$1::uuid AND id=$2::uuid FOR KEY SHARE", owner, project_id);
    if (parent.empty()) throw ApiError(drogon::k404NotFound, "Project not found");
    return one(transaction->execSqlSync(
        "INSERT INTO tasks AS t (project_id,title) VALUES ($1::uuid,$2) RETURNING " + columns,
        project_id, title));
}

Json::Value TaskRepository::list(const std::string& owner, const std::string& project_id) const {
    auto rows = drogon::app().getDbClient()->execSqlSync(
        "SELECT " + columns + " FROM tasks t JOIN projects p ON p.id=t.project_id "
        "WHERE p.user_id=$1::uuid AND p.id=$2::uuid ORDER BY t.created_at,t.id", owner, project_id);
    Json::Value result(Json::arrayValue);
    for (const auto& row : rows) result.append(task(row));
    return result;
}

Json::Value TaskRepository::get(const std::string& owner, const std::string& id) const {
    return one(drogon::app().getDbClient()->execSqlSync(
        "SELECT " + columns + " FROM tasks t JOIN projects p ON p.id=t.project_id "
        "WHERE p.user_id=$1::uuid AND t.id=$2::uuid", owner, id));
}

Json::Value TaskRepository::update(const std::string& owner, const std::string& id,
                                   const Json::Value& patch) const {
    auto transaction = drogon::app().getDbClient()->newTransaction();
    auto rows = transaction->execSqlSync(
        "SELECT t.is_paused FROM tasks t JOIN projects p ON p.id=t.project_id "
        "WHERE p.user_id=$1::uuid AND t.id=$2::uuid FOR UPDATE OF t", owner, id);
    if (rows.empty()) throw ApiError(drogon::k404NotFound, "Task not found");
    if (patch.get("is_completed", false).asBool()) {
        auto active = transaction->execSqlSync(
            "SELECT 1 FROM time_entries WHERE task_id=$1::uuid AND ended_at IS NULL LIMIT 1", id);
        if (rows[0]["is_paused"].as<bool>() || !active.empty())
            throw ApiError(drogon::k409Conflict, "Stop the timer before completing the task");
    }
    return one(transaction->execSqlSync(
        "UPDATE tasks AS t SET title=CASE WHEN $3::boolean THEN $4 ELSE t.title END, "
        "is_completed=CASE WHEN $5::boolean THEN $6::boolean ELSE t.is_completed END "
        "FROM projects p WHERE p.id=t.project_id AND p.user_id=$1::uuid AND t.id=$2::uuid RETURNING " + columns,
        owner, id, patch.isMember("title"), patch.get("title", "").asString(),
        patch.isMember("is_completed"), patch.get("is_completed", false).asBool()));
}

void TaskRepository::remove(const std::string& owner, const std::string& id) const {
    auto transaction = drogon::app().getDbClient()->newTransaction();
    auto rows = transaction->execSqlSync(
        "SELECT t.id FROM tasks t JOIN projects p ON p.id=t.project_id "
        "WHERE p.user_id=$1::uuid AND t.id=$2::uuid FOR UPDATE OF t", owner, id);
    if (rows.empty()) throw ApiError(drogon::k404NotFound, "Task not found");
    // Блокировка задачи не позволяет параллельно добавить интервал через FK.
    auto entries = transaction->execSqlSync("SELECT 1 FROM time_entries WHERE task_id=$1::uuid LIMIT 1", id);
    if (!entries.empty()) throw ApiError(drogon::k409Conflict, "Task has time entries; complete it instead of deleting");
    transaction->execSqlSync("DELETE FROM tasks WHERE id=$1::uuid", id);
}
}
