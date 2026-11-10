#include "ReportController.h"
namespace time_tracker {
namespace {
Json::Value queryParameters(const drogon::HttpRequestPtr& req) {
    Json::Value query(Json::objectValue);
    for (const auto& [key,value]:req->getParameters()) query[key]=value;
    return query;
}
}
void registerReportRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<ReportService>& reports,
                          const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers) {
    drogon::app().registerHandler("/reports",
        [auth,reports,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
            dispatch(workers,std::move(reply),[auth,reports,req] {
                return reports->report(auth->authenticate(req->getHeader("authorization")).id,queryParameters(req));
            });
        },{drogon::Get});
}
}
