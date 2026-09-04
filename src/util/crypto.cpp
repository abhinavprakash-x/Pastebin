#include "crypto.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace pastebin::util {

namespace {

// SHA-256 internal functions and constants
inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
    return (e & f) ^ (~e & g);
}

inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
    return (a & b) ^ (a & c) ^ (b & c);
}

inline uint32_t sig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t sig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t theta0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t theta1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::vector<uint8_t> sha256_bytes(const uint8_t* data, size_t len) {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint64_t total_bits = static_cast<uint64_t>(len) * 8;
    std::vector<uint8_t> padded(data, data + len);
    padded.push_back(0x80);

    while ((padded.size() % 64) != 56) {
        padded.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<uint8_t>((total_bits >> (i * 8)) & 0xff));
    }

    for (size_t chunk = 0; chunk < padded.size(); chunk += 64) {
        uint32_t W[64];
        for (int i = 0; i < 16; ++i) {
            W[i] = (static_cast<uint32_t>(padded[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(padded[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            W[i] = theta1(W[i - 2]) + W[i - 7] + theta0(W[i - 15]) + W[i - 16];
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3],
                 e = H[4], f = H[5], g = H[6], h = H[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t T1 = h + sig1(e) + choose(e, f, g) + K[i] + W[i];
            uint32_t T2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    std::vector<uint8_t> digest(32);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4]     = static_cast<uint8_t>((H[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<uint8_t>((H[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<uint8_t>((H[i] >> 8) & 0xff);
        digest[i * 4 + 3] = static_cast<uint8_t>(H[i] & 0xff);
    }
    return digest;
}

std::vector<uint8_t> hmac_sha256_bytes(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len) {
    uint8_t k_pad[64];
    std::memset(k_pad, 0, sizeof(k_pad));

    if (key_len > 64) {
        auto key_hash = sha256_bytes(key, key_len);
        std::memcpy(k_pad, key_hash.data(), 32);
    } else {
        std::memcpy(k_pad, key, key_len);
    }

    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = k_pad[i] ^ 0x36;
        opad[i] = k_pad[i] ^ 0x5c;
    }

    std::vector<uint8_t> inner_data(ipad, ipad + 64);
    inner_data.insert(inner_data.end(), data, data + data_len);
    auto inner_hash = sha256_bytes(inner_data.data(), inner_data.size());

    std::vector<uint8_t> outer_data(opad, opad + 64);
    outer_data.insert(outer_data.end(), inner_hash.begin(), inner_hash.end());
    return sha256_bytes(outer_data.data(), outer_data.size());
}

std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

} // namespace

std::string Crypto::sha256_hex(const std::string& input) {
    auto digest = sha256_bytes(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    return bytes_to_hex(digest);
}

std::string Crypto::hmac_sha256_hex(const std::string& key, const std::string& data) {
    auto digest = hmac_sha256_bytes(
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size()
    );
    return bytes_to_hex(digest);
}

std::string Crypto::pbkdf2_sha256_hex(const std::string& password, const std::string& salt, uint32_t iterations, size_t key_len) {
    const uint8_t* pwd = reinterpret_cast<const uint8_t*>(password.data());
    size_t pwd_len = password.size();
    const uint8_t* s = reinterpret_cast<const uint8_t*>(salt.data());
    size_t salt_len = salt.size();

    std::vector<uint8_t> result;
    result.reserve(key_len);

    uint32_t block_index = 1;
    while (result.size() < key_len) {
        // U1 = HMAC(password, salt || INT(block_index))
        std::vector<uint8_t> salt_plus_index(s, s + salt_len);
        salt_plus_index.push_back(static_cast<uint8_t>((block_index >> 24) & 0xff));
        salt_plus_index.push_back(static_cast<uint8_t>((block_index >> 16) & 0xff));
        salt_plus_index.push_back(static_cast<uint8_t>((block_index >> 8) & 0xff));
        salt_plus_index.push_back(static_cast<uint8_t>(block_index & 0xff));

        auto u_prev = hmac_sha256_bytes(pwd, pwd_len, salt_plus_index.data(), salt_plus_index.size());
        auto u_xor = u_prev;

        for (uint32_t iter = 1; iter < iterations; ++iter) {
            u_prev = hmac_sha256_bytes(pwd, pwd_len, u_prev.data(), u_prev.size());
            for (size_t k = 0; k < 32; ++k) {
                u_xor[k] ^= u_prev[k];
            }
        }

        size_t to_copy = std::min<size_t>(32, key_len - result.size());
        result.insert(result.end(), u_xor.begin(), u_xor.begin() + to_copy);
        block_index++;
    }

    return bytes_to_hex(result);
}

std::string Crypto::generate_random_hex(size_t byte_count) {
    std::random_device rd;
    std::vector<uint8_t> bytes(byte_count);
    for (size_t i = 0; i < byte_count; ++i) {
        bytes[i] = static_cast<uint8_t>(rd() & 0xff);
    }
    return bytes_to_hex(bytes);
}

std::string Crypto::generate_session_token() {
    return generate_random_hex(32); // 256-bit token = 64 hex chars
}

bool Crypto::constant_time_equals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    uint8_t result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return result == 0;
}

} // namespace pastebin::util
