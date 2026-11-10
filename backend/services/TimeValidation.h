#pragma once

#include "Validation.h"
#include <charconv>
#include <cstdint>
#include <ctime>

namespace time_tracker {
// Количество микросекунд от Unix epoch. Вход всегда содержит часовой пояс.
inline std::int64_t timestampMicros(const std::string& text) {
    static const std::regex pattern(
        R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})(\.([0-9]{1,6}))?(Z|([+-])([0-9]{2}):([0-9]{2}))$)");
    std::smatch m;
    if (!std::regex_match(text, m, pattern))
        throw ApiError(drogon::k400BadRequest, "Expected ISO 8601 timestamp with Z or timezone offset");
    const int year=std::stoi(m[1]), month=std::stoi(m[2]), day=std::stoi(m[3]);
    const int hour=std::stoi(m[4]), minute=std::stoi(m[5]), second=std::stoi(m[6]);
    if (year<1 || month<1 || month>12 || day<1 || day>31 || hour>23 || minute>59 || second>59)
        throw ApiError(drogon::k400BadRequest, "Invalid calendar timestamp");
    std::tm calendar{};
    calendar.tm_year=year-1900; calendar.tm_mon=month-1; calendar.tm_mday=day;
    calendar.tm_hour=hour; calendar.tm_min=minute; calendar.tm_sec=second;
    const auto seconds=timegm(&calendar);
    if (calendar.tm_year!=year-1900 || calendar.tm_mon!=month-1 || calendar.tm_mday!=day)
        throw ApiError(drogon::k400BadRequest, "Invalid calendar date");
    int offset=0;
    if (m[9]!="Z") {
        const int hours=std::stoi(m[11]), minutes=std::stoi(m[12]);
        if (hours>14 || minutes>59 || (hours==14 && minutes!=0))
            throw ApiError(drogon::k400BadRequest, "Invalid timezone offset");
        offset=(hours*60+minutes)*60*(m[10]=="-" ? -1 : 1);
    }
    auto fraction=m[8].str();
    fraction.append(6-fraction.size(),'0');
    return (static_cast<std::int64_t>(seconds)-offset)*1000000 + std::stoll(fraction);
}
inline void validDate(const std::string& date) {
    static const std::regex pattern(R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}$)");
    if (!std::regex_match(date, pattern) || date.substr(0,4)=="9999")
        throw ApiError(drogon::k400BadRequest, "Expected date YYYY-MM-DD (year 0001 to 9998)");
    timestampMicros(date+"T00:00:00Z");
}
inline void checkUuid(const std::string& id) {
    if (!validUuid(id)) throw ApiError(drogon::k400BadRequest, "Invalid UUID");
}
inline int queryInteger(const Json::Value& query, const char* key, int fallback, int minimum, int maximum) {
    if (!query.isMember(key)) return fallback;
    const auto value=textField(query,key);
    int number=0;
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),number);
    if (error!=std::errc{} || end!=value.data()+value.size() || number<minimum || number>maximum)
        throw ApiError(drogon::k400BadRequest, std::string("Invalid query parameter: ")+key);
    return number;
}
}
