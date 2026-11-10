#pragma once
#include <stdexcept>
#include <drogon/HttpTypes.h>
namespace time_tracker {
struct ApiError : std::runtime_error {
    drogon::HttpStatusCode status;
    ApiError(drogon::HttpStatusCode code, const std::string& message)
        : std::runtime_error(message), status(code) {}
};
}
