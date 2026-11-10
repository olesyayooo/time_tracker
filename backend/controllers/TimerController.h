#pragma once
#include "HttpSupport.h"
#include "services/AuthService.h"
#include "services/TimerService.h"
namespace time_tracker {
void registerTimerRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<TimerService>& timer,
                         const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers);
}
