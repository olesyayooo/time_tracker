#include "Config.h"

#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace time_tracker {
namespace {

std::string required(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error(std::string("Missing environment variable: ") + name);
    }
    return value;
}

std::string withDefault(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? value : fallback;
}

std::uint16_t port(const char* name, const char* fallback) {
    const auto value = withDefault(name, fallback);
    unsigned int number = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size() || number == 0 || number > 65535) {
        // Не включаем значение переменной в сообщение об ошибке.
        throw std::runtime_error(std::string("Invalid port in environment variable: ") + name);
    }
    return static_cast<std::uint16_t>(number);
}

} // namespace

Config Config::fromEnvironment() {
    const auto secret = required("JWT_SECRET");
    if (secret.size() < 32) throw std::runtime_error("JWT_SECRET must contain at least 32 bytes");
    const auto algorithm = withDefault("ALGORITHM", "HS256");
    if (algorithm != "HS256") throw std::runtime_error("Only HS256 is supported");
    const auto hours = port("TOKEN_EXPIRE_HOURS", "1");
    if (hours > 24) throw std::runtime_error("TOKEN_EXPIRE_HOURS must be between 1 and 24");
    return Config{
        withDefault("DB_HOST", "127.0.0.1"),
        port("DB_PORT", "5432"),
        required("POSTGRES_DB"),
        required("POSTGRES_USER"),
        required("POSTGRES_PASSWORD"),
        withDefault("BACKEND_HOST", "127.0.0.1"),
        port("BACKEND_PORT", "8080"),
        secret, algorithm, hours
    };
}

} // namespace time_tracker
