#pragma once

#include <string>
#include <random>
#include <mutex>

namespace pastebin::util {

class KeyGenerator {
public:
    explicit KeyGenerator(size_t default_length = 6);

    // Generates a random alphanumeric key of given length
    std::string generate(size_t length = 0);

    // Validates whether a key contains only allowed alphanumeric characters
    static bool validate_key(const std::string& key);

    static constexpr const char* ALPHABET =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    static constexpr size_t MIN_KEY_LENGTH = 3;
    static constexpr size_t MAX_KEY_LENGTH = 64;

private:
    size_t default_length_;
    std::mt19937_64 rng_;
    std::uniform_int_distribution<size_t> dist_;
    std::mutex rng_mutex_;
};

} // namespace pastebin::util
