#include "key_generator.hpp"
#include <cstring>
#include <cctype>

namespace pastebin::util {

KeyGenerator::KeyGenerator(size_t default_length)
    : default_length_(default_length == 0 ? 6 : default_length),
      dist_(0, std::strlen(ALPHABET) - 1) {
    std::random_device rd;
    // Seed with high entropy 64-bit seed from random_device
    uint64_t seed = (static_cast<uint64_t>(rd()) << 32) | rd();
    rng_.seed(seed);
}

std::string KeyGenerator::generate(size_t length) {
    if (length == 0) {
        length = default_length_;
    }

    std::string key;
    key.reserve(length);

    std::lock_guard<std::mutex> lock(rng_mutex_);
    for (size_t i = 0; i < length; ++i) {
        key.push_back(ALPHABET[dist_(rng_)]);
    }

    return key;
}

bool KeyGenerator::validate_key(const std::string& key) {
    if (key.empty() || key.length() < MIN_KEY_LENGTH || key.length() > MAX_KEY_LENGTH) {
        return false;
    }

    for (char c : key) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

} // namespace pastebin::util
