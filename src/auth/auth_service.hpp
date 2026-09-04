#pragma once

#include "user.hpp"
#include "../db/database.hpp"
#include <memory>
#include <string>
#include <optional>

namespace pastebin::auth {

struct AuthResult {
    bool success{false};
    std::string token;
    User user;
    std::string error_message;
};

class AuthService {
public:
    explicit AuthService(std::shared_ptr<db::Database> db);

    AuthResult register_user(const std::string& username, const std::string& password);
    AuthResult login_user(const std::string& username, const std::string& password);
    bool logout_user(const std::string& token);

    // Authenticates a Bearer token or session token
    std::optional<User> authenticate_token(const std::string& token);

    static bool validate_username(const std::string& username);
    static bool validate_password(const std::string& password);

    static constexpr int64_t SESSION_DURATION_SECONDS = 7 * 24 * 3600; // 7 days

private:
    std::shared_ptr<db::Database> db_;
};

} // namespace pastebin::auth
