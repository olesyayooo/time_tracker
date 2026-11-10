#include "TimerService.h"
#include "TimeFilters.h"
#include <drogon/drogon.h>
#include <limits>

namespace time_tracker {
namespace {
void validRange(const std::string& start,const std::string& end,const std::string& now) {
    const auto a=timestampMicros(start), b=timestampMicros(end);
    if (b<=a) throw ApiError(drogon::k400BadRequest,"ended_at must be later than started_at");
    if (b>timestampMicros(now)) throw ApiError(drogon::k400BadRequest,"Manual entries must not be in the future");
}
}
Json::Value TimerService::control(const std::string& owner,const std::string& operation,const Json::Value& body) const {
    allowedFields(body,{"task_id"});
    const auto id=textField(body,"task_id"); checkUuid(id);
    return entries_.withUserLock(owner,[&](const auto& db) {
        auto task=entries_.task(db,owner,id);
        // Сравниваем канонический UUID, прочитанный из БД.
        const auto taskId=task["id"].asString();
        auto active=entries_.active(db,owner);
        const bool isPaused=task["is_paused"].asBool();
        if (operation=="start" || operation=="resume") {
            if (task["is_completed"].asBool()) throw ApiError(drogon::k409Conflict,"Task is completed");
            if (!active.isNull()) throw ApiError(drogon::k409Conflict,"User already has an active timer");
            if ((operation=="resume")!=isPaused)
                throw ApiError(drogon::k409Conflict,isPaused ? "Use resume for a paused task" : "Task is not paused");
            const auto now=entries_.now(db);
            if (entries_.overlaps(db,owner,now,"")) throw ApiError(drogon::k409Conflict,"Time interval overlaps existing work");
            auto result=entries_.insert(db,owner,taskId,now,"");
            entries_.paused(db,taskId,false);
            return result;
        }
        if (operation!="pause" && operation!="stop") throw ApiError(drogon::k400BadRequest,"Unknown timer operation");
        if (operation=="stop" && isPaused) {
            Json::Value result;
            result["task"]=entries_.paused(db,taskId,false);
            result["time_entry"]=Json::Value();
            return result;
        }
        if (active.isNull() || active["task_id"].asString()!=taskId)
            throw ApiError(drogon::k409Conflict,"Task has no running timer");
        const auto now=entries_.now(db);
        if (timestampMicros(now)<=timestampMicros(active["started_at"].asString()))
            throw ApiError(drogon::k409Conflict,"Timer duration must be positive");
        auto closed=entries_.update(db,owner,active["id"].asString(),active["started_at"].asString(),now);
        auto updatedTask=entries_.paused(db,taskId,operation=="pause");
        if (operation=="pause") return closed;
        Json::Value result;
        result["task"]=updatedTask; result["time_entry"]=closed;
        return result;
    });
}
Json::Value TimerService::active(const std::string& owner) const {
    return entries_.active(drogon::app().getDbClient(),owner);
}
Json::Value TimerService::list(const std::string& owner,const Json::Value& query) const {
    allowedFields(query,{"from","to","project_id","task_id","limit","offset"});
    auto filter=scopeFilters(owner,query);
    if (query.isMember("from")) { filter.from=textField(query,"from"); timestampMicros(filter.from); }
    if (query.isMember("to")) { filter.to=textField(query,"to"); timestampMicros(filter.to); }
    if (!filter.from.empty() && !filter.to.empty() && timestampMicros(filter.from)>=timestampMicros(filter.to))
        throw ApiError(drogon::k400BadRequest,"from must be earlier than to");
    filter.limit=queryInteger(query,"limit",100,1,500);
    filter.offset=queryInteger(query,"offset",0,0,std::numeric_limits<int>::max());
    return entries_.list(owner,filter);
}
Json::Value TimerService::create(const std::string& owner,const Json::Value& body) const {
    allowedFields(body,{"task_id","started_at","ended_at"});
    const auto id=textField(body,"task_id"); checkUuid(id);
    const auto start=textField(body,"started_at"), end=textField(body,"ended_at");
    timestampMicros(start); timestampMicros(end);
    return entries_.withUserLock(owner,[&](const auto& db) {
        auto task=entries_.task(db,owner,id);
        if (task["is_completed"].asBool()) throw ApiError(drogon::k409Conflict,"Task is completed");
        validRange(start,end,entries_.now(db));
        if (entries_.overlaps(db,owner,start,end)) throw ApiError(drogon::k409Conflict,"Time interval overlaps existing work");
        return entries_.insert(db,owner,task["id"].asString(),start,end);
    });
}
Json::Value TimerService::update(const std::string& owner,const std::string& id,const Json::Value& body) const {
    checkUuid(id); allowedFields(body,{"started_at","ended_at"});
    if (body.empty()) throw ApiError(drogon::k400BadRequest,"Patch must not be empty");
    // Проверка типа до обращения к БД; null не означает остановку таймера.
    if (body.isMember("started_at")) timestampMicros(textField(body,"started_at"));
    if (body.isMember("ended_at")) timestampMicros(textField(body,"ended_at"));
    return entries_.withUserLock(owner,[&](const auto& db) {
        auto entry=entries_.entry(db,owner,id);
        if (entry["ended_at"].isNull()) throw ApiError(drogon::k409Conflict,"Stop the timer before editing its entry");
        auto start=body.get("started_at",entry["started_at"]).asString();
        auto end=body.get("ended_at",entry["ended_at"]).asString();
        validRange(start,end,entries_.now(db));
        if (entries_.overlaps(db,owner,start,end,id)) throw ApiError(drogon::k409Conflict,"Time interval overlaps existing work");
        return entries_.update(db,owner,id,start,end);
    });
}
void TimerService::remove(const std::string& owner,const std::string& id) const {
    checkUuid(id);
    entries_.withUserLock(owner,[&](const auto& db) {
        auto entry=entries_.entry(db,owner,id);
        if (entry["ended_at"].isNull()) throw ApiError(drogon::k409Conflict,"Stop the timer before deleting its entry");
        entries_.remove(db,owner,id);
        return Json::Value();
    });
}
}
