#pragma once
#include "repositories/ReportRepository.h"
namespace time_tracker {
class ReportService {
public:
    Json::Value report(const std::string& owner,const Json::Value& query) const;
private:
    ReportRepository reports_;
};
}
