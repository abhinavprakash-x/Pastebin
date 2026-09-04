#pragma once

#include "../auth/user.hpp"
#include "../paste/paste_metadata.hpp"

#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <mutex>
#include <filesystem>

struct sqlite3;

namespace pastebin::db {

class Database {
public:
    explicit Database(std::filesystem::path db_path);
    ~Database();

    bool initialize();

    // User operations
    std::optional<int64_t> create_user(const std::string& username, const std::string& password_hash, const std::string& salt);
    std::optional<auth::User> get_user_by_username(const std::string& username);
    std::optional<auth::User> get_user_by_id(int64_t user_id);

    // Session operations
    bool create_session(const std::string& token, int64_t user_id, int64_t expires_at);
    std::optional<auth::Session> get_session(const std::string& token);
    bool delete_session(const std::string& token);
    void cleanup_expired_sessions();

    // Paste metadata operations
    bool save_paste_metadata(const paste::PasteMetadata& meta);
    std::optional<paste::PasteMetadata> get_paste_metadata(const std::string& key);
    std::vector<paste::PasteMetadata> get_pastes_by_user_id(int64_t user_id);
    bool delete_paste_metadata(const std::string& key, int64_t user_id);

private:
    std::filesystem::path db_path_;
    sqlite3* db_{nullptr};
    std::mutex mutex_;

    bool run_migrations();
    static std::string current_iso_time();
};

} // namespace pastebin::db
