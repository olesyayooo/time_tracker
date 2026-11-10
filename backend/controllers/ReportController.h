#pragma once
#include "HttpSupport.h"
#include "services/AuthService.h"
#include "services/ReportService.h"
namespace time_tracker {
void registerReportRoutes(const std::shared_ptr<AuthService>& auth,const std::shared_ptr<ReportService>& reports,
                          const std::shared_ptr<trantor::ConcurrentTaskQueue>& workers);
}
