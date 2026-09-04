#pragma once

#include <string>
#include <cstdint>

namespace pastebin::auth {

struct User {
    int64_t id{0};
    std::string username;
    std::string password_hash;
    std::string salt;
    std::string created_at;
};

struct Session {
    std::string token;
    int64_t user_id{0};
    int64_t expires_at{0};
    std::string created_at;
};

} // namespace pastebin::auth
