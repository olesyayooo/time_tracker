#pragma once
#include "TimeEntryRepository.h"
namespace time_tracker {
class ReportRepository {
public:
    Json::Value report(const std::string& owner,const EntryFilter& filter,const std::string& group) const;
};
}
