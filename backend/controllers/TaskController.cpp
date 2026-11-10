#include "TaskController.h"

namespace time_tracker {
void registerTaskRoutes(const std::shared_ptr<AuthService>& auth, const std::shared_ptr<TaskService>& tasks,
                        const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers) {
    drogon::app().registerHandler("/projects/{1}/tasks",
        [auth, tasks, workers](const drogon::HttpRequestPtr& req, Reply&& reply, std::string project_id) {
            dispatch(workers, std::move(reply), [auth, tasks, req, project_id] {
                auto user = auth->authenticate(req->getHeader("authorization"));
                return tasks->create(user.id, project_id, jsonBody(req));
            }, drogon::k201Created);
        }, {drogon::Post});
    drogon::app().registerHandler("/projects/{1}/tasks",
        [auth, tasks, workers](const drogon::HttpRequestPtr& req, Reply&& reply, std::string project_id) {
            dispatch(workers, std::move(reply), [auth, tasks, req, project_id] {
                return tasks->list(auth->authenticate(req->getHeader("authorization")).id, project_id);
            });
        }, {drogon::Get});
    drogon::app().registerHandler("/tasks/{1}",
        [auth, tasks, workers](const drogon::HttpRequestPtr& req, Reply&& reply, std::string id) {
            dispatch(workers, std::move(reply), [auth, tasks, req, id] {
                return tasks->get(auth->authenticate(req->getHeader("authorization")).id, id);
            });
        }, {drogon::Get});
    drogon::app().registerHandler("/tasks/{1}",
        [auth, tasks, workers](const drogon::HttpRequestPtr& req, Reply&& reply, std::string id) {
            dispatch(workers, std::move(reply), [auth, tasks, req, id] {
                auto user = auth->authenticate(req->getHeader("authorization"));
                return tasks->update(user.id, id, jsonBody(req));
            });
        }, {drogon::Patch});
    drogon::app().registerHandler("/tasks/{1}",
        [auth, tasks, workers](const drogon::HttpRequestPtr& req, Reply&& reply, std::string id) {
            dispatch(workers, std::move(reply), [auth, tasks, req, id] {
                tasks->remove(auth->authenticate(req->getHeader("authorization")).id, id);
                return Json::Value();
            }, drogon::k204NoContent);
        }, {drogon::Delete});
}
}
