#include "paste_service.hpp"
#include "../util/logger.hpp"

namespace pastebin::paste {

PasteService::PasteService(
    std::shared_ptr<storage::PasteStorage> storage,
    std::shared_ptr<db::Database> db,
    std::shared_ptr<util::KeyGenerator> key_generator,
    size_t max_paste_size
) : storage_(std::move(storage)),
    db_(std::move(db)),
    key_generator_(key_generator ? std::move(key_generator) : std::make_shared<util::KeyGenerator>()),
    max_paste_size_(max_paste_size) {
}

std::optional<std::string> PasteService::create_paste(
    const std::string& content,
    int64_t user_id,
    bool is_public,
    bool is_encrypted,
    const std::string& title,
    size_t key_len
) {
    if (content.empty()) {
        LOG_WARN("Cannot create empty paste");
        return std::nullopt;
    }

    if (content.size() > max_paste_size_) {
        LOG_WARN("Paste size " + std::to_string(content.size()) + " exceeds maximum limit " + std::to_string(max_paste_size_));
        return std::nullopt;
    }

    // Unauthenticated / guest users can only create public pastes
    if (user_id == 0) {
        is_public = true;
    }

    // Try generating a non-colliding key (retry up to 10 times)
    constexpr int MAX_ATTEMPTS = 10;
    std::string key;
    bool found_unique = false;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        key = key_generator_->generate(key_len);
        if (!storage_->exists(key)) {
            found_unique = true;
            break;
        }
        LOG_WARN("Collision detected for key: " + key + ", retrying...");
    }

    if (!found_unique) {
        LOG_ERROR("Failed to generate a unique key after " + std::to_string(MAX_ATTEMPTS) + " attempts");
        return std::nullopt;
    }

    if (!storage_->save(key, content)) {
        LOG_ERROR("Failed to save paste with key: " + key);
        return std::nullopt;
    }

    // Save metadata if database is present
    if (db_) {
        PasteMetadata meta;
        meta.key = key;
        meta.user_id = user_id;
        meta.is_public = is_public;
        meta.is_encrypted = is_encrypted;
        meta.title = title;
        db_->save_paste_metadata(meta);
    }

    LOG_INFO("Created paste key: " + key + " (owner: " + (user_id == 0 ? "guest" : std::to_string(user_id))
             + ", public: " + (is_public ? "yes" : "no")
             + ", encrypted: " + (is_encrypted ? "yes" : "no") + ")");
    return key;
}

PasteFetchResult PasteService::get_paste_with_access(const std::string& key, int64_t requesting_user_id) {
    PasteFetchResult res;

    if (!util::KeyGenerator::validate_key(key)) {
        res.status = AccessResult::INVALID_KEY;
        return res;
    }

    if (db_) {
        auto meta_opt = db_->get_paste_metadata(key);
        if (meta_opt) {
            res.metadata = *meta_opt;
            // Privacy check: If not public and not owner -> forbidden!
            if (!meta_opt->is_public) {
                if (requesting_user_id == 0 || meta_opt->user_id != requesting_user_id) {
                    LOG_WARN("Access denied for private paste: " + key + " (requesting user: " + std::to_string(requesting_user_id) + ")");
                    res.status = AccessResult::FORBIDDEN_PRIVATE;
                    return res;
                }
            }
        }
    }

    auto content_opt = storage_->load(key);
    if (!content_opt) {
        res.status = AccessResult::NOT_FOUND;
        return res;
    }

    res.status = AccessResult::ALLOWED;
    res.content = *content_opt;
    return res;
}

std::optional<std::string> PasteService::get_paste(const std::string& key) {
    auto res = get_paste_with_access(key, 0);
    if (res.status == AccessResult::ALLOWED) {
        return res.content;
    }
    return std::nullopt;
}

std::optional<PasteMetadata> PasteService::get_metadata(const std::string& key) {
    if (!db_ || !util::KeyGenerator::validate_key(key)) {
        return std::nullopt;
    }
    return db_->get_paste_metadata(key);
}

std::vector<UserPasteSummary> PasteService::get_user_pastes(int64_t user_id) {
    std::vector<UserPasteSummary> summaries;
    if (!db_ || user_id == 0) return summaries;

    auto list = db_->get_pastes_by_user_id(user_id);
    summaries.reserve(list.size());

    for (const auto& meta : list) {
        UserPasteSummary summary;
        summary.key = meta.key;
        summary.is_public = meta.is_public;
        summary.is_encrypted = meta.is_encrypted;
        summary.title = meta.title;
        summary.created_at = meta.created_at;

        // Fetch small snippet if available
        auto content_opt = storage_->load(meta.key);
        if (content_opt) {
            if (meta.is_encrypted) {
                summary.snippet = "[Encrypted Content]";
            } else {
                summary.snippet = content_opt->length() > 80 ? content_opt->substr(0, 80) + "..." : *content_opt;
            }
        }
        summaries.push_back(std::move(summary));
    }

    return summaries;
}

bool PasteService::delete_paste(const std::string& key, int64_t user_id) {
    if (!util::KeyGenerator::validate_key(key)) return false;

    if (db_) {
        auto meta_opt = db_->get_paste_metadata(key);
        if (!meta_opt) return false;

        // Ensure user is owner (or user_id == 0 if admin/override)
        if (user_id != 0 && meta_opt->user_id != user_id) {
            LOG_WARN("User " + std::to_string(user_id) + " attempted to delete unowned paste " + key);
            return false;
        }

        db_->delete_paste_metadata(key, user_id);
    }

    if (storage_) {
        storage_->remove(key);
    }

    LOG_INFO("Deleted paste: " + key + " by user: " + std::to_string(user_id));
    return true;
}

bool PasteService::exists(const std::string& key) {
    if (!util::KeyGenerator::validate_key(key)) {
        return false;
    }
    return storage_->exists(key);
}

} // namespace pastebin::paste
