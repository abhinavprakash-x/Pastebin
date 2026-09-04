#include "database.hpp"
#include "../../third_party/sqlite3/sqlite3.h"
#include "../util/logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace pastebin::db {

std::string Database::current_iso_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

Database::Database(std::filesystem::path db_path) : db_path_(std::move(db_path)) {
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_path_.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(db_path_.parent_path(), ec);
    }

    int rc = sqlite3_open(db_path_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open SQLite database " + db_path_.string() + ": " + sqlite3_errmsg(db_));
        return false;
    }

    // Enable WAL mode for better concurrency and performance
    char* err_msg = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg);
    if (err_msg) {
        sqlite3_free(err_msg);
    }

    return run_migrations();
}

bool Database::run_migrations() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL COLLATE NOCASE,
            password_hash TEXT NOT NULL,
            salt TEXT NOT NULL,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
        CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);

        CREATE TABLE IF NOT EXISTS pastes_meta (
            key TEXT PRIMARY KEY,
            user_id INTEGER DEFAULT 0,
            is_public INTEGER NOT NULL DEFAULT 1,
            is_encrypted INTEGER NOT NULL DEFAULT 0,
            title TEXT,
            created_at TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_pastes_user_id ON pastes_meta(user_id);
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        LOG_ERROR("Failed to run SQLite schema migrations: " + err);
        return false;
    }

    LOG_INFO("SQLite database initialized at " + db_path_.string());
    return true;
}

std::optional<int64_t> Database::create_user(const std::string& username, const std::string& password_hash, const std::string& salt) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO users (username, password_hash, salt, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare create_user statement: " + std::string(sqlite3_errmsg(db_)));
        return std::nullopt;
    }

    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_WARN("Failed to insert user " + username + " (possibly already exists): " + sqlite3_errmsg(db_));
        return std::nullopt;
    }

    return sqlite3_last_insert_rowid(db_);
}

std::optional<auth::User> Database::get_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT id, username, password_hash, salt, created_at FROM users WHERE username = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auth::User user;
        user.id = sqlite3_column_int64(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<auth::User> Database::get_user_by_id(int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT id, username, password_hash, salt, created_at FROM users WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, user_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auth::User user;
        user.id = sqlite3_column_int64(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::create_session(const std::string& token, int64_t user_id, int64_t expires_at) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT OR REPLACE INTO sessions (token, user_id, expires_at, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, expires_at);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<auth::Session> Database::get_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT token, user_id, expires_at, created_at FROM sessions WHERE token = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auth::Session sess;
        sess.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sess.user_id = sqlite3_column_int64(stmt, 1);
        sess.expires_at = sqlite3_column_int64(stmt, 2);
        sess.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        sqlite3_finalize(stmt);
        return sess;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::delete_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM sessions WHERE token = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

void Database::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const char* sql = "DELETE FROM sessions WHERE expires_at < ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool Database::save_paste_metadata(const paste::PasteMetadata& meta) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT OR REPLACE INTO pastes_meta (key, user_id, is_public, is_encrypted, title, created_at) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare save_paste_metadata: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    std::string now = meta.created_at.empty() ? current_iso_time() : meta.created_at;
    sqlite3_bind_text(stmt, 1, meta.key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, meta.user_id);
    sqlite3_bind_int(stmt, 3, meta.is_public ? 1 : 0);
    sqlite3_bind_int(stmt, 4, meta.is_encrypted ? 1 : 0);
    sqlite3_bind_text(stmt, 5, meta.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<paste::PasteMetadata> Database::get_paste_metadata(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT key, user_id, is_public, is_encrypted, title, created_at FROM pastes_meta WHERE key = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        paste::PasteMetadata meta;
        meta.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.user_id = sqlite3_column_int64(stmt, 1);
        meta.is_public = sqlite3_column_int(stmt, 2) != 0;
        meta.is_encrypted = sqlite3_column_int(stmt, 3) != 0;
        const char* title_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        meta.title = title_ptr ? title_ptr : "";
        meta.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        sqlite3_finalize(stmt);
        return meta;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<paste::PasteMetadata> Database::get_pastes_by_user_id(int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<paste::PasteMetadata> results;
    const char* sql = "SELECT key, user_id, is_public, is_encrypted, title, created_at FROM pastes_meta WHERE user_id = ? ORDER BY created_at DESC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    sqlite3_bind_int64(stmt, 1, user_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        paste::PasteMetadata meta;
        meta.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.user_id = sqlite3_column_int64(stmt, 1);
        meta.is_public = sqlite3_column_int(stmt, 2) != 0;
        meta.is_encrypted = sqlite3_column_int(stmt, 3) != 0;
        const char* title_ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        meta.title = title_ptr ? title_ptr : "";
        meta.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        results.push_back(std::move(meta));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::delete_paste_metadata(const std::string& key, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM pastes_meta WHERE key = ? AND (user_id = ? OR ? = 0);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, user_id);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && changes > 0;
}

} // namespace pastebin::db
