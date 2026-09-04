#pragma once

#include "paste_storage.hpp"
#include <filesystem>
#include <mutex>
#include <shared_mutex>

namespace pastebin::storage {

class FileStorage : public PasteStorage {
public:
    explicit FileStorage(std::filesystem::path base_dir, bool enable_sharding = false);
    ~FileStorage() override = default;

    bool save(const std::string& key, const std::string& content) override;
    std::optional<std::string> load(const std::string& key) override;
    bool exists(const std::string& key) override;
    bool remove(const std::string& key) override;

    // Helper to get resolved filesystem path for a valid key
    std::optional<std::filesystem::path> get_key_path(const std::string& key) const;

private:
    std::filesystem::path base_dir_;
    bool enable_sharding_;
    mutable std::shared_mutex rw_mutex_;
};

} // namespace pastebin::storage
