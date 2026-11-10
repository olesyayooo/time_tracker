#include "TimeEntryRepository.h"
#include "services/ApiError.h"
#include <drogon/drogon.h>
#include <future>

namespace time_tracker {
namespace {
const std::string columns="e.id,e.user_id,e.task_id,"
    "to_char(e.started_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS started_at,"
    "to_char(e.ended_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS ended_at";
Json::Value readEntry(const drogon::orm::Row& row) {
    Json::Value result;
    for (const auto* key : {"id","user_id","task_id","started_at"}) result[key]=row[key].as<std::string>();
    result["ended_at"]=row["ended_at"].isNull() ? Json::Value() : Json::Value(row["ended_at"].as<std::string>());
    return result;
}
Json::Value readTask(const drogon::orm::Row& row) {
    Json::Value result;
    for (const auto* key : {"id","project_id","title","created_at"}) result[key]=row[key].as<std::string>();
    result["is_completed"]=row["is_completed"].as<bool>();
    result["is_paused"]=row["is_paused"].as<bool>();
    return result;
}
const std::string taskColumns="t.id,t.project_id,t.title,t.is_completed,t.is_paused,"
    "to_char(t.created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS created_at";
}
Json::Value TimeEntryRepository::withUserLock(const std::string& owner,
    const std::function<Json::Value(const Db&)>& action) const {
    auto transaction=drogon::app().getDbClient()->newTransaction();
    Json::Value result;
    try {
        auto users=transaction->execSqlSync("SELECT id FROM users WHERE id=$1::uuid FOR UPDATE",owner);
        if (users.empty()) throw ApiError(drogon::k401Unauthorized,"User no longer exists");
        result=action(transaction);
    } catch (...) {
        transaction->rollback();
        throw;
    }
    auto committed=std::make_shared<std::promise<bool>>();
    auto future=committed->get_future();
    transaction->setCommitCallback([committed](bool success) { committed->set_value(success); });
    transaction.reset(); // В Drogon освобождение Transaction отправляет COMMIT.
    if (!future.get()) throw ApiError(drogon::k500InternalServerError,"Transaction could not be committed");
    return result;
}
Json::Value TimeEntryRepository::task(const Db& db,const std::string& owner,const std::string& id) const {
    auto rows=db->execSqlSync("SELECT "+taskColumns+" FROM tasks t JOIN projects p ON p.id=t.project_id "
        "WHERE p.user_id=$1::uuid AND t.id=$2::uuid FOR UPDATE OF t",owner,id);
    if (rows.empty()) throw ApiError(drogon::k404NotFound,"Task not found");
    return readTask(rows[0]);
}
Json::Value TimeEntryRepository::active(const Db& db,const std::string& owner) const {
    auto rows=db->execSqlSync("SELECT "+columns+" FROM time_entries e WHERE e.user_id=$1::uuid AND e.ended_at IS NULL",owner);
    return rows.empty() ? Json::Value() : readEntry(rows[0]);
}
Json::Value TimeEntryRepository::entry(const Db& db,const std::string& owner,const std::string& id) const {
    auto rows=db->execSqlSync("SELECT "+columns+" FROM time_entries e WHERE e.user_id=$1::uuid AND e.id=$2::uuid FOR UPDATE",owner,id);
    if (rows.empty()) throw ApiError(drogon::k404NotFound,"Time entry not found");
    return readEntry(rows[0]);
}
std::string TimeEntryRepository::now(const Db& db) const {
    // clock_timestamp вычисляется ПОСЛЕ ожидания блокировок, в отличие от transaction now().
    return db->execSqlSync("SELECT to_char(clock_timestamp() AT TIME ZONE 'UTC',"
        "'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS value")[0]["value"].as<std::string>();
}
bool TimeEntryRepository::overlaps(const Db& db,const std::string& owner,const std::string& start,
    const std::string& end,const std::string& except) const {
    return !db->execSqlSync("SELECT 1 FROM time_entries WHERE user_id=$1::uuid "
        "AND ($4='' OR id<>NULLIF($4,'')::uuid) "
        "AND tstzrange(started_at,ended_at,'[)') && tstzrange($2::timestamptz,NULLIF($3,'')::timestamptz,'[)') LIMIT 1",
        owner,start,end,except).empty();
}
Json::Value TimeEntryRepository::insert(const Db& db,const std::string& owner,const std::string& task_id,
    const std::string& start,const std::string& end) const {
    return readEntry(db->execSqlSync("INSERT INTO time_entries AS e (user_id,task_id,started_at,ended_at) "
        "VALUES ($1::uuid,$2::uuid,$3::timestamptz,NULLIF($4,'')::timestamptz) RETURNING "+columns,
        owner,task_id,start,end)[0]);
}
Json::Value TimeEntryRepository::update(const Db& db,const std::string& owner,const std::string& id,
    const std::string& start,const std::string& end) const {
    return readEntry(db->execSqlSync("UPDATE time_entries AS e SET started_at=$3::timestamptz,ended_at=$4::timestamptz "
        "WHERE e.user_id=$1::uuid AND e.id=$2::uuid RETURNING "+columns,owner,id,start,end)[0]);
}
void TimeEntryRepository::remove(const Db& db,const std::string& owner,const std::string& id) const {
    db->execSqlSync("DELETE FROM time_entries WHERE user_id=$1::uuid AND id=$2::uuid",owner,id);
}
Json::Value TimeEntryRepository::paused(const Db& db,const std::string& id,bool paused) const {
    return readTask(db->execSqlSync("UPDATE tasks AS t SET is_paused=$2::boolean WHERE t.id=$1::uuid RETURNING "+taskColumns,id,paused)[0]);
}
Json::Value TimeEntryRepository::list(const std::string& owner,const EntryFilter& f) const {
    auto rows=drogon::app().getDbClient()->execSqlSync("SELECT "+columns+
        " FROM time_entries e JOIN tasks t ON t.id=e.task_id JOIN projects p ON p.id=t.project_id "
        "WHERE e.user_id=$1::uuid AND p.user_id=$1::uuid "
        "AND ($2='' OR e.ended_at IS NULL OR e.ended_at>NULLIF($2,'')::timestamptz) "
        "AND ($3='' OR e.started_at<NULLIF($3,'')::timestamptz) "
        "AND ($4='' OR p.id=NULLIF($4,'')::uuid) AND ($5='' OR t.id=NULLIF($5,'')::uuid) "
        "ORDER BY e.started_at DESC,e.id DESC LIMIT $6::integer OFFSET $7::integer",
        owner,f.from,f.to,f.project_id,f.task_id,f.limit,f.offset);
    Json::Value result(Json::arrayValue);
    for (const auto& row:rows) result.append(readEntry(row));
    return result;
}
}
