#pragma once
#include "services/AuthService.h"
#include "HttpSupport.h"
namespace time_tracker {
void registerAuthRoutes(const std::shared_ptr<AuthService>& auth,
                        const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers);
}
