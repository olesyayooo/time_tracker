#pragma once
#include "services/AuthService.h"
#include "services/ProjectService.h"
#include "HttpSupport.h"
namespace time_tracker {
void registerProjectRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<ProjectService>& projects,
                           const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers);
}
