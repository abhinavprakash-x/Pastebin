#pragma once

#include <string>
#include <optional>

namespace pastebin::storage {

class PasteStorage {
public:
    virtual ~PasteStorage() = default;

    // Saves paste content under the specified key
    virtual bool save(const std::string& key, const std::string& content) = 0;

    // Loads paste content for the specified key, returns nullopt if not found or invalid
    virtual std::optional<std::string> load(const std::string& key) = 0;

    // Checks if a paste with the specified key exists
    virtual bool exists(const std::string& key) = 0;

    // Removes paste file/record
    virtual bool remove(const std::string& key) = 0;
};

} // namespace pastebin::storage
