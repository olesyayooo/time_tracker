#pragma once
#include "services/ApiError.h"
#include <drogon/drogon.h>
#include <trantor/utils/ConcurrentTaskQueue.h>
namespace time_tracker {
using Reply=std::function<void(const drogon::HttpResponsePtr&)>;
inline Json::Value jsonBody(const drogon::HttpRequestPtr& request) {
    auto body=request->getJsonObject();
    if (!body || !body->isObject()) throw ApiError(drogon::k400BadRequest,"Expected a JSON object");
    return *body;
}
inline void dispatch(const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers, Reply reply,
                     std::function<Json::Value()> action, drogon::HttpStatusCode success=drogon::k200OK) {
    workers->runTaskInQueue([reply=std::move(reply),action=std::move(action),success] {
        Json::Value body;
        auto status=success;
        try { body=action(); }
        catch (const ApiError& error) { status=error.status; body["error"]=error.what(); }
        catch (const std::exception&) { status=drogon::k500InternalServerError; body["error"]="Internal server error"; }
        auto response=status==drogon::k204NoContent ? drogon::HttpResponse::newHttpResponse()
                                                  : drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(status);
        if (status==drogon::k401Unauthorized) response->addHeader("WWW-Authenticate","Bearer");
        reply(response);
    });
}
}
