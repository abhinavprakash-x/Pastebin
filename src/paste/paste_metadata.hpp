#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace pastebin::paste {

struct PasteMetadata {
    std::string key;
    int64_t user_id{0}; // 0 = anonymous / guest
    bool is_public{true};
    bool is_encrypted{false};
    std::string title;
    std::string created_at;
};

struct UserPasteSummary {
    std::string key;
    bool is_public{true};
    bool is_encrypted{false};
    std::string title;
    std::string created_at;
    std::string snippet;
};

} // namespace pastebin::paste
