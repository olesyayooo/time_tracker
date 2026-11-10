#pragma once
#include "ApiError.h"
#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <regex>
#include <string>
namespace time_tracker {
inline std::size_t characters(const std::string& value) {
    return std::count_if(value.begin(), value.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; });
}
inline std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
inline std::string textField(const Json::Value& body, const char* key) {
    if (!body.isMember(key) || !body[key].isString())
        throw ApiError(drogon::k400BadRequest, std::string(key) + " must be a string");
    return body[key].asString();
}
inline void allowedFields(const Json::Value& body, std::initializer_list<std::string> fields) {
    for (const auto& key : body.getMemberNames())
        if (std::find(fields.begin(), fields.end(), key) == fields.end())
            throw ApiError(drogon::k400BadRequest, "Unknown field: " + key);
}
inline std::string validName(const std::string& input, std::size_t limit) {
    auto name = trim(input);
    if (name.empty() || characters(name) > limit || name.find('\0') != std::string::npos)
        throw ApiError(drogon::k400BadRequest, "Name is empty or too long");
    return name;
}
inline bool validUuid(const std::string& id) {
    static const std::regex pattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(id, pattern);
}
inline std::string validEmail(const std::string& input) {
    auto email = trim(input);
    static const std::regex pattern(R"(^[A-Za-z0-9!#$%&'*+/=?^_`{|}~.-]+@[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)+$)");
    auto at = email.find('@');
    if (email.size() > 254 || at == std::string::npos || at > 64 ||
        !std::regex_match(email, pattern) || email.front() == '.' || email[at-1] == '.' || email.substr(0, at).find("..") != std::string::npos)
        throw ApiError(drogon::k400BadRequest, "Invalid email format");
    std::transform(email.begin(), email.end(), email.begin(), [](unsigned char c) { return std::tolower(c); });
    return email;
}
inline void validPassword(const std::string& password) {
    if (characters(password) < 8 || characters(password) > 20 || password.find('\0') != std::string::npos)
        throw ApiError(drogon::k400BadRequest, "Password must contain 8 to 20 characters");
}
}
