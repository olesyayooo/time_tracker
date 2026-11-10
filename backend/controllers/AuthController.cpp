#include "AuthController.h"
namespace time_tracker {
void registerAuthRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers) {
    drogon::app().registerHandler("/auth/register",[auth,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
        dispatch(workers,std::move(reply),[auth,req] { return auth->registerUser(jsonBody(req)); },drogon::k201Created);
    },{drogon::Post});
    drogon::app().registerHandler("/auth/login",[auth,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
        dispatch(workers,std::move(reply),[auth,req] { return auth->login(jsonBody(req)); });
    },{drogon::Post});
    drogon::app().registerHandler("/users/me",[auth,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
        dispatch(workers,std::move(reply),[auth,req] { return AuthService::publicUser(auth->authenticate(req->getHeader("authorization"))); });
    },{drogon::Get});
}
}
