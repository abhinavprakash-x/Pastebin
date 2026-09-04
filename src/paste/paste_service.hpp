#pragma once

#include "paste_metadata.hpp"
#include "../storage/paste_storage.hpp"
#include "../util/key_generator.hpp"
#include "../db/database.hpp"

#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace pastebin::paste {

enum class AccessResult {
    ALLOWED,
    FORBIDDEN_PRIVATE,
    NOT_FOUND,
    INVALID_KEY
};

struct PasteFetchResult {
    AccessResult status{AccessResult::NOT_FOUND};
    std::string content;
    PasteMetadata metadata;
};

class PasteService {
public:
    static constexpr size_t DEFAULT_MAX_PASTE_SIZE = 10 * 1024 * 1024; // 10 MB

    explicit PasteService(
        std::shared_ptr<storage::PasteStorage> storage,
        std::shared_ptr<db::Database> db = nullptr,
        std::shared_ptr<util::KeyGenerator> key_generator = nullptr,
        size_t max_paste_size = DEFAULT_MAX_PASTE_SIZE
    );

    // Creates a new paste with privacy and encryption metadata
    std::optional<std::string> create_paste(
        const std::string& content,
        int64_t user_id = 0,
        bool is_public = true,
        bool is_encrypted = false,
        const std::string& title = "",
        size_t key_len = 6
    );

    // Retrieves paste content and enforces public/private access control
    PasteFetchResult get_paste_with_access(const std::string& key, int64_t requesting_user_id = 0);

    // Backward-compatible simple get
    std::optional<std::string> get_paste(const std::string& key);

    // Get metadata for a paste
    std::optional<PasteMetadata> get_metadata(const std::string& key);

    // Retrieves all pastes created by a user
    std::vector<UserPasteSummary> get_user_pastes(int64_t user_id);

    // Deletes a paste (must be owned by user_id)
    bool delete_paste(const std::string& key, int64_t user_id);

    // Checks if a paste exists
    bool exists(const std::string& key);

    size_t get_max_paste_size() const { return max_paste_size_; }

private:
    std::shared_ptr<storage::PasteStorage> storage_;
    std::shared_ptr<db::Database> db_;
    std::shared_ptr<util::KeyGenerator> key_generator_;
    size_t max_paste_size_;
};

} // namespace pastebin::paste
