#include "UserRepository.h"
#include <drogon/drogon.h>
namespace time_tracker {
namespace {
User readUser(const drogon::orm::Row& row) {
    return {row["id"].as<std::string>(), row["name"].as<std::string>(),
            row["email"].as<std::string>(), row["password_hash"].as<std::string>()};
}
}
std::optional<User> UserRepository::create(const std::string& name, const std::string& email, const std::string& hash) const {
    auto rows = drogon::app().getDbClient()->execSqlSync("INSERT INTO users (name,email,password_hash) VALUES ($1,$2,$3) ON CONFLICT (lower(email)) DO NOTHING RETURNING *", name,email,hash);
    if (rows.empty()) return std::nullopt;
    return readUser(rows[0]);
}
std::optional<User> UserRepository::byEmail(const std::string& email) const {
    auto rows = drogon::app().getDbClient()->execSqlSync("SELECT * FROM users WHERE lower(email)=lower($1)",email);
    if (rows.empty()) return std::nullopt;
    return readUser(rows[0]);
}
std::optional<User> UserRepository::byId(const std::string& id) const {
    auto rows = drogon::app().getDbClient()->execSqlSync("SELECT * FROM users WHERE id=$1::uuid",id);
    if (rows.empty()) return std::nullopt;
    return readUser(rows[0]);
}
}
