#include "AuthService.h"
#include "Validation.h"
#include <jwt-cpp/jwt.h>
#include <sodium.h>
#include <chrono>
namespace time_tracker {
namespace {
std::string hashPassword(const std::string& password) {
    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str_alg(hash, password.data(), password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw std::runtime_error("Password hashing failed");
    return hash;
}
}
AuthService::AuthService(Config config)
    : config_(std::move(config)) {
    if (sodium_init() < 0) throw std::runtime_error("Sodium init failed");
    dummy_hash_ = hashPassword("dummy-not-a-user-password");
}
Json::Value AuthService::publicUser(const User& user) {
    Json::Value result;
    result["id"]=user.id; result["name"]=user.name; result["email"]=user.email;
    return result;
}
Json::Value AuthService::tokenResponse(const User& user) const {
    const auto now = std::chrono::system_clock::now();
    const auto token = jwt::create().set_type("JWT").set_issuer("time-tracker")
        .set_subject(user.id).set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(config_.token_expire_hours))
        .sign(jwt::algorithm::hs256{config_.jwt_secret});
    Json::Value result;
    result["access_token"]=token; result["token_type"]="Bearer";
    result["expires_in"]=config_.token_expire_hours * 3600;
    result["user"]=publicUser(user);
    return result;
}
Json::Value AuthService::registerUser(const Json::Value& body) const {
    allowedFields(body,{"name","email","password"});
    const auto email=validEmail(textField(body,"email"));
    const auto password=textField(body,"password");
    validPassword(password);
    const auto name=validName(body.isMember("name") ? textField(body,"name") : email.substr(0,email.find('@')),100);
    auto user = users_.create(name,email,hashPassword(password));
    if (!user) throw ApiError(drogon::k409Conflict,"Email already registered");
    return tokenResponse(*user);
}
Json::Value AuthService::login(const Json::Value& body) const {
    allowedFields(body,{"email","password"});
    const auto email=validEmail(textField(body,"email"));
    const auto password=textField(body,"password");
    validPassword(password);
    auto user=users_.byEmail(email);
    const auto& hash=user ? user->password_hash : dummy_hash_;
    const bool matches=crypto_pwhash_str_verify(hash.c_str(),password.data(),password.size()) == 0;
    if (!user || !matches) throw ApiError(drogon::k401Unauthorized,"Invalid email or password");
    return tokenResponse(*user);
}
User AuthService::authenticate(const std::string& authorization) const {
    try {
        if (authorization.rfind("Bearer ",0)!=0 || authorization.size()>8192) throw std::runtime_error("Bearer required");
        auto decoded=jwt::decode(authorization.substr(7));
        jwt::verify().allow_algorithm(jwt::algorithm::hs256{config_.jwt_secret})
            .with_issuer("time-tracker").verify(decoded);
        if (!decoded.has_expires_at() || !decoded.has_issued_at() || !validUuid(decoded.get_subject()))
            throw std::runtime_error("Claims missing");
        auto user=users_.byId(decoded.get_subject());
        if (user) return *user;
    } catch (const drogon::orm::DrogonDbException&) { throw; }
      catch (const std::exception&) {}
    throw ApiError(drogon::k401Unauthorized,"Invalid or expired access token");
}
}
