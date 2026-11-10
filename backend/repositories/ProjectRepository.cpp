#include "ProjectRepository.h"
#include <drogon/drogon.h>
#include "services/ApiError.h"
namespace time_tracker {
namespace {
const std::string columns = "id,user_id,name,is_completed,to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS created_at";
Json::Value project(const drogon::orm::Row& row) {
    Json::Value value;
    for (const auto* key : {"id","user_id","name","created_at"}) value[key]=row[key].as<std::string>();
    value["is_completed"]=row["is_completed"].as<bool>();
    return value;
}
Json::Value one(const drogon::orm::Result& rows) {
    if (rows.empty()) throw ApiError(drogon::k404NotFound,"Project not found");
    return project(rows[0]);
}
}
Json::Value ProjectRepository::create(const std::string& owner,const std::string& name) const {
    return one(drogon::app().getDbClient()->execSqlSync("INSERT INTO projects (user_id,name) VALUES ($1::uuid,$2) RETURNING "+columns,owner,name));
}
Json::Value ProjectRepository::list(const std::string& owner) const {
    auto rows=drogon::app().getDbClient()->execSqlSync("SELECT "+columns+" FROM projects WHERE user_id=$1::uuid ORDER BY created_at,id",owner);
    Json::Value result(Json::arrayValue);
    for (const auto& row:rows) result.append(project(row));
    return result;
}
Json::Value ProjectRepository::get(const std::string& owner,const std::string& id) const {
    return one(drogon::app().getDbClient()->execSqlSync("SELECT "+columns+" FROM projects WHERE user_id=$1::uuid AND id=$2::uuid",owner,id));
}
Json::Value ProjectRepository::update(const std::string& owner,const std::string& id,const Json::Value& patch) const {
    // Один UPDATE: параллельные изменения отдельных полей не затирают друг друга.
    return one(drogon::app().getDbClient()->execSqlSync("UPDATE projects SET name=CASE WHEN $3::boolean THEN $4 ELSE name END, "
        "is_completed=CASE WHEN $5::boolean THEN $6::boolean ELSE is_completed END "
        "WHERE user_id=$1::uuid AND id=$2::uuid RETURNING "+columns,
        owner,id,patch.isMember("name"),patch.get("name","").asString(),
        patch.isMember("is_completed"),patch.get("is_completed",false).asBool()));
}
void ProjectRepository::remove(const std::string& owner,const std::string& id) const {
    auto transaction = drogon::app().getDbClient()->newTransaction();
    auto rows = transaction->execSqlSync(
        "SELECT id FROM projects WHERE user_id=$1::uuid AND id=$2::uuid FOR UPDATE", owner, id);
    if (rows.empty()) throw ApiError(drogon::k404NotFound, "Project not found");
    auto tasks = transaction->execSqlSync("SELECT 1 FROM tasks WHERE project_id=$1::uuid LIMIT 1", id);
    if (!tasks.empty()) throw ApiError(drogon::k409Conflict, "Project has tasks; delete them first or complete the project");
    transaction->execSqlSync("DELETE FROM projects WHERE id=$1::uuid", id);
}
}
