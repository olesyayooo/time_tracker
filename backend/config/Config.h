#pragma once

#include <cstdint>
#include <string>

namespace time_tracker {

struct Config {
    std::string db_host;
    std::uint16_t db_port;
    std::string db_name;
    std::string db_user;
    std::string db_password;
    std::string backend_host;
    std::uint16_t backend_port;

    std::string jwt_secret;
    std::string algorithm;
    int token_expire_hours;

    // Читает окружение процесса. Сам файл .env не загружает.
    static Config fromEnvironment();
};

} // namespace time_tracker
