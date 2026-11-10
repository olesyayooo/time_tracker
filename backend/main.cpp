#include "config/Config.h"
#include "controllers/AuthController.h"
#include "controllers/ProjectController.h"
#include "controllers/TaskController.h"
#include "controllers/TimerController.h"
#include "controllers/ReportController.h"

#include <drogon/drogon.h>
#include <exception>
#include <functional>
#include <iostream>
#include <utility>

int main() {
    try {
        const auto config = time_tracker::Config::fromEnvironment();
        auto& app = drogon::app();
        app.setLogLevel(trantor::Logger::kWarn);
        app.addListener(config.backend_host, config.backend_port);
        app.setThreadNum(2);
        app.addDbClient(drogon::orm::PostgresConfig{
            .host = config.db_host,
            .port = config.db_port,
            .databaseName = config.db_name,
            .username = config.db_user,
            .password = config.db_password,
            .connectionNumber = 2,
            .name = "default",
            .isFast = false,
            .characterSet = "utf8",
            .timeout = 5.0,
            .autoBatch = false,
            .connectOptions = {}
        });

        app.registerHandler("/health",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                // Один из двух обработчиков завершит HTTP-запрос после ответа БД.
                auto reply = [callback = std::move(callback)](bool available) {
                    Json::Value body;
                    body["status"] = available ? "ok" : "unavailable";
                    body["database"] = available ? "ok" : "unavailable";
                    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
                    response->setStatusCode(available ? drogon::k200OK : drogon::k503ServiceUnavailable);
                    callback(response);
                };
                drogon::app().getDbClient()->execSqlAsync("SELECT 1",
                    [reply](const drogon::orm::Result&) { reply(true); },
                    [reply](const drogon::orm::DrogonDbException&) { reply(false); });
            }, {drogon::Get});

        auto workers = std::make_shared<trantor::ConcurrentTaskQueue>(2, "api-workers");
        auto auth = std::make_shared<time_tracker::AuthService>(config);
        auto projects = std::make_shared<time_tracker::ProjectService>();
        time_tracker::registerAuthRoutes(auth, workers);
        time_tracker::registerProjectRoutes(auth, projects, workers);
        auto tasks = std::make_shared<time_tracker::TaskService>();
        time_tracker::registerTaskRoutes(auth, tasks, workers);
        auto timer = std::make_shared<time_tracker::TimerService>();
        auto reports = std::make_shared<time_tracker::ReportService>();
        time_tracker::registerTimerRoutes(auth, timer, workers);
        time_tracker::registerReportRoutes(auth, reports, workers);

        std::cout << "Backend: http://" << config.backend_host << ':'
                  << config.backend_port << "\n";
        app.run();
    } catch (const std::exception&) {
        // Тексты исключений библиотек могут содержать параметры подключения.
        std::cerr << "Backend startup failed. Check environment, database and port availability.\n";
        return 1;
    }
}
