#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pastebin::util {

class Crypto {
public:
    // Computes SHA-256 hash of data, returns 64-character lowercase hex string
    static std::string sha256_hex(const std::string& input);

    // Computes HMAC-SHA256 of data with key, returns 64-character lowercase hex string
    static std::string hmac_sha256_hex(const std::string& key, const std::string& data);

    // PBKDF2-HMAC-SHA256 key derivation / password hashing
    static std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt, uint32_t iterations = 10000, size_t key_len = 32);

    // Generates cryptographically secure random hex string of given byte length (output is 2*bytes chars)
    static std::string generate_random_hex(size_t byte_count = 16);

    // Generates a random session token
    static std::string generate_session_token();

    // Constant-time string comparison to prevent timing attacks
    static bool constant_time_equals(const std::string& a, const std::string& b);
};

} // namespace pastebin::util
