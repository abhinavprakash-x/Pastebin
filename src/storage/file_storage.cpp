#include "file_storage.hpp"
#include "../util/key_generator.hpp"
#include "../util/logger.hpp"

#include <fstream>
#include <system_error>

namespace pastebin::storage {

FileStorage::FileStorage(std::filesystem::path base_dir, bool enable_sharding)
    : base_dir_(std::move(base_dir)), enable_sharding_(enable_sharding) {
    std::error_code ec;
    std::filesystem::create_directories(base_dir_, ec);
    if (ec) {
        LOG_ERROR("Failed to create base storage directory " + base_dir_.string() + ": " + ec.message());
    } else {
        LOG_INFO("File storage initialized at " + base_dir_.string());
    }
}

std::optional<std::filesystem::path> FileStorage::get_key_path(const std::string& key) const {
    if (!util::KeyGenerator::validate_key(key)) {
        LOG_WARN("Path traversal or invalid key rejected: " + key);
        return std::nullopt;
    }

    if (enable_sharding_ && key.length() >= 4) {
        std::string prefix1 = key.substr(0, 2);
        std::string prefix2 = key.substr(2, 2);
        return base_dir_ / prefix1 / prefix2 / (key + ".txt");
    }

    return base_dir_ / (key + ".txt");
}

bool FileStorage::save(const std::string& key, const std::string& content) {
    auto path_opt = get_key_path(key);
    if (!path_opt) {
        return false;
    }

    const auto& target_path = *path_opt;

    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    try {
        std::error_code ec;
        if (target_path.has_parent_path()) {
            std::filesystem::create_directories(target_path.parent_path(), ec);
            if (ec) {
                LOG_ERROR("Failed to create directories for " + target_path.string() + ": " + ec.message());
                return false;
            }
        }

        // Write directly to the target file
        std::ofstream ofs(target_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            LOG_ERROR("Failed to open file for writing: " + target_path.string());
            return false;
        }

        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();

        if (!ofs.good()) {
            LOG_ERROR("Failed during write to file: " + target_path.string());
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception while saving paste " + key + ": " + ex.what());
        return false;
    }
}

std::optional<std::string> FileStorage::load(const std::string& key) {
    auto path_opt = get_key_path(key);
    if (!path_opt) {
        return std::nullopt;
    }

    const auto& target_path = *path_opt;

    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    try {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(target_path, ec) || ec) {
            return std::nullopt;
        }

        std::ifstream ifs(target_path, std::ios::in | std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            return std::nullopt;
        }

        auto file_size = ifs.tellg();
        if (file_size < 0) {
            return std::nullopt;
        }

        std::string result;
        result.resize(static_cast<size_t>(file_size));

        ifs.seekg(0, std::ios::beg);
        if (file_size > 0) {
            ifs.read(&result[0], file_size);
            if (!ifs.good()) {
                return std::nullopt;
            }
        }

        return result;
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception while loading paste " + key + ": " + ex.what());
        return std::nullopt;
    }
}

bool FileStorage::exists(const std::string& key) {
    auto path_opt = get_key_path(key);
    if (!path_opt) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    std::error_code ec;
    return std::filesystem::is_regular_file(*path_opt, ec) && !ec;
}

bool FileStorage::remove(const std::string& key) {
    auto path_opt = get_key_path(key);
    if (!path_opt) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    std::error_code ec;
    return std::filesystem::remove(*path_opt, ec) && !ec;
}

} // namespace pastebin::storage
