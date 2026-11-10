#pragma once

#include "HttpSupport.h"
#include "services/AuthService.h"
#include "services/TaskService.h"

namespace time_tracker {
void registerTaskRoutes(const std::shared_ptr<AuthService>& auth,
                        const std::shared_ptr<TaskService>& tasks,
                        const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers);
}
