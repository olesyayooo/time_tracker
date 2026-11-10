#pragma once
#include "config/Config.h"
#include "repositories/UserRepository.h"
#include <json/json.h>
namespace time_tracker {
class AuthService {
public:
    explicit AuthService(Config config);
    Json::Value registerUser(const Json::Value& body) const;
    Json::Value login(const Json::Value& body) const;
    User authenticate(const std::string& authorization) const;
    static Json::Value publicUser(const User& user);
private:
    Json::Value tokenResponse(const User& user) const;
    UserRepository users_;
    Config config_;
    std::string dummy_hash_;
};
}
