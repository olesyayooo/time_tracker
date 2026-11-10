#pragma once
#include "repositories/TimeEntryRepository.h"
namespace time_tracker {
class TimerService {
public:
    Json::Value control(const std::string& owner,const std::string& operation,const Json::Value& body) const;
    Json::Value active(const std::string& owner) const;
    Json::Value list(const std::string& owner,const Json::Value& query) const;
    Json::Value create(const std::string& owner,const Json::Value& body) const;
    Json::Value update(const std::string& owner,const std::string& id,const Json::Value& body) const;
    void remove(const std::string& owner,const std::string& id) const;
private:
    TimeEntryRepository entries_;
};
}
