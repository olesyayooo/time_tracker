#include "TimerController.h"

namespace time_tracker {
namespace {
Json::Value queryParameters(const drogon::HttpRequestPtr& req) {
    Json::Value query(Json::objectValue);
    for (const auto& [key,value]:req->getParameters()) query[key]=value;
    return query;
}
}
void registerTimerRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<TimerService>& timer,
                         const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers) {
    for (const std::string operation:{"start","pause","resume","stop"}) {
        drogon::app().registerHandler("/time-entries/"+operation,
            [auth,timer,workers,operation](const drogon::HttpRequestPtr& req,Reply&& reply) {
                dispatch(workers,std::move(reply),[auth,timer,req,operation] {
                    return timer->control(auth->authenticate(req->getHeader("authorization")).id,operation,jsonBody(req));
                },operation=="start" || operation=="resume" ? drogon::k201Created : drogon::k200OK);
            },{drogon::Post});
    }
    drogon::app().registerHandler("/time-entries/active",
        [auth,timer,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
            dispatch(workers,std::move(reply),[auth,timer,req] {
                return timer->active(auth->authenticate(req->getHeader("authorization")).id);
            });
        },{drogon::Get});
    drogon::app().registerHandler("/time-entries",
        [auth,timer,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
            dispatch(workers,std::move(reply),[auth,timer,req] {
                return timer->list(auth->authenticate(req->getHeader("authorization")).id,queryParameters(req));
            });
        },{drogon::Get});
    drogon::app().registerHandler("/time-entries",
        [auth,timer,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
            dispatch(workers,std::move(reply),[auth,timer,req] {
                return timer->create(auth->authenticate(req->getHeader("authorization")).id,jsonBody(req));
            },drogon::k201Created);
        },{drogon::Post});
    drogon::app().registerHandler("/time-entries/{1}",
        [auth,timer,workers](const drogon::HttpRequestPtr& req,Reply&& reply,std::string id) {
            dispatch(workers,std::move(reply),[auth,timer,req,id] {
                return timer->update(auth->authenticate(req->getHeader("authorization")).id,id,jsonBody(req));
            });
        },{drogon::Patch});
    drogon::app().registerHandler("/time-entries/{1}",
        [auth,timer,workers](const drogon::HttpRequestPtr& req,Reply&& reply,std::string id) {
            dispatch(workers,std::move(reply),[auth,timer,req,id] {
                timer->remove(auth->authenticate(req->getHeader("authorization")).id,id);
                return Json::Value();
            },drogon::k204NoContent);
        },{drogon::Delete});
}
}
