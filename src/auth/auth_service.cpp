#include "auth_service.hpp"
#include "../util/crypto.hpp"
#include "../util/logger.hpp"

#include <chrono>
#include <cctype>

namespace pastebin::auth {

AuthService::AuthService(std::shared_ptr<db::Database> db) : db_(std::move(db)) {
}

bool AuthService::validate_username(const std::string& username) {
    if (username.length() < 3 || username.length() > 32) {
        return false;
    }
    for (char c : username) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

bool AuthService::validate_password(const std::string& password) {
    return password.length() >= 6 && password.length() <= 128;
}

AuthResult AuthService::register_user(const std::string& username, const std::string& password) {
    AuthResult res;

    if (!validate_username(username)) {
        res.error_message = "Username must be 3-32 characters (letters, numbers, underscores, hyphens).";
        return res;
    }

    if (!validate_password(password)) {
        res.error_message = "Password must be at least 6 characters long.";
        return res;
    }

    if (db_->get_user_by_username(username).has_value()) {
        res.error_message = "Username is already taken.";
        return res;
    }

    // Generate 128-bit salt
    std::string salt = util::Crypto::generate_random_hex(16);
    // Hash password with PBKDF2
    std::string hash = util::Crypto::pbkdf2_sha256_hex(password, salt, 10000);

    auto user_id_opt = db_->create_user(username, hash, salt);
    if (!user_id_opt) {
        res.error_message = "Failed to create user account in database.";
        return res;
    }

    auto user_opt = db_->get_user_by_id(*user_id_opt);
    if (!user_opt) {
        res.error_message = "Failed to retrieve newly created user.";
        return res;
    }

    // Create session token
    std::string token = util::Crypto::generate_session_token();
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    int64_t expires_at = now_epoch + SESSION_DURATION_SECONDS;

    if (!db_->create_session(token, *user_id_opt, expires_at)) {
        res.error_message = "User registered but failed to initialize session.";
        return res;
    }

    res.success = true;
    res.token = token;
    res.user = *user_opt;
    LOG_INFO("Registered new user: " + username + " (id: " + std::to_string(*user_id_opt) + ")");
    return res;
}

AuthResult AuthService::login_user(const std::string& username, const std::string& password) {
    AuthResult res;

    auto user_opt = db_->get_user_by_username(username);
    if (!user_opt) {
        res.error_message = "Invalid username or password.";
        return res;
    }

    const auto& user = *user_opt;
    std::string computed_hash = util::Crypto::pbkdf2_sha256_hex(password, user.salt, 10000);

    if (!util::Crypto::constant_time_equals(computed_hash, user.password_hash)) {
        res.error_message = "Invalid username or password.";
        LOG_WARN("Failed login attempt for user: " + username);
        return res;
    }

    // Generate new session token
    std::string token = util::Crypto::generate_session_token();
    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    int64_t expires_at = now_epoch + SESSION_DURATION_SECONDS;

    if (!db_->create_session(token, user.id, expires_at)) {
        res.error_message = "Failed to create session token.";
        return res;
    }

    res.success = true;
    res.token = token;
    res.user = user;
    LOG_INFO("User logged in successfully: " + username);
    return res;
}

bool AuthService::logout_user(const std::string& token) {
    if (token.empty()) return false;
    return db_->delete_session(token);
}

std::optional<User> AuthService::authenticate_token(const std::string& token) {
    if (token.empty()) return std::nullopt;

    auto session_opt = db_->get_session(token);
    if (!session_opt) {
        return std::nullopt;
    }

    auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (session_opt->expires_at < now_epoch) {
        db_->delete_session(token);
        return std::nullopt;
    }

    return db_->get_user_by_id(session_opt->user_id);
}

} // namespace pastebin::auth
