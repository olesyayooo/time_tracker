#include "ProjectController.h"
namespace time_tracker {
void registerProjectRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<ProjectService>& projects,
                          const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers) {
    drogon::app().registerHandler("/projects",[auth,projects,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
        dispatch(workers,std::move(reply),[auth,projects,req] {
            auto user=auth->authenticate(req->getHeader("authorization"));
            return projects->create(user.id,jsonBody(req));
        },drogon::k201Created);
    },{drogon::Post});
    drogon::app().registerHandler("/projects",[auth,projects,workers](const drogon::HttpRequestPtr& req,Reply&& reply) {
        dispatch(workers,std::move(reply),[auth,projects,req] {
            return projects->list(auth->authenticate(req->getHeader("authorization")).id);
        });
    },{drogon::Get});
    drogon::app().registerHandler("/projects/{1}",[auth,projects,workers](const drogon::HttpRequestPtr& req,Reply&& reply,std::string id) {
        dispatch(workers,std::move(reply),[auth,projects,req,id] {
            return projects->get(auth->authenticate(req->getHeader("authorization")).id,id);
        });
    },{drogon::Get});
    drogon::app().registerHandler("/projects/{1}",[auth,projects,workers](const drogon::HttpRequestPtr& req,Reply&& reply,std::string id) {
        dispatch(workers,std::move(reply),[auth,projects,req,id] {
            auto user=auth->authenticate(req->getHeader("authorization"));
            return projects->update(user.id,id,jsonBody(req));
        });
    },{drogon::Patch});
    drogon::app().registerHandler("/projects/{1}",[auth,projects,workers](const drogon::HttpRequestPtr& req,Reply&& reply,std::string id) {
        dispatch(workers,std::move(reply),[auth,projects,req,id] {
            projects->remove(auth->authenticate(req->getHeader("authorization")).id,id);
            return Json::Value();
        },drogon::k204NoContent);
    },{drogon::Delete});
}
}
