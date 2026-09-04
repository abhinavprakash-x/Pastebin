#include "../src/auth/auth_service.hpp"
#include "../src/db/database.hpp"
#include "../src/util/crypto.hpp"

#include <iostream>
#include <cassert>
#include <filesystem>

void cleanup(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove(p, ec);
}

void test_crypto_primitives() {
    std::string text = "The quick brown fox jumps over the lazy dog";
    std::string hash = pastebin::util::Crypto::sha256_hex(text);
    assert(hash == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");

    // HMAC
    std::string key = "key";
    std::string data = "The quick brown fox jumps over the lazy dog";
    std::string hmac = pastebin::util::Crypto::hmac_sha256_hex(key, data);
    assert(hmac == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");

    // PBKDF2
    std::string salt = "randomsalt123456";
    std::string pbkdf2 = pastebin::util::Crypto::pbkdf2_sha256_hex("password123", salt, 1000);
    assert(pbkdf2 == "3b9b2a0f7efe440b26d40dc44d939d099aeb72d30fbed0d49f6a05f909408e6c");

    std::cout << "[PASS] test_crypto_primitives" << std::endl;
}

void test_user_registration_and_login() {
    std::filesystem::path db_file = "test_auth.db";
    cleanup(db_file);

    auto db = std::make_shared<pastebin::db::Database>(db_file);
    assert(db->initialize());

    pastebin::auth::AuthService auth_service(db);

    // 1. Register User
    auto reg_res = auth_service.register_user("alice", "SuperSecret123");
    assert(reg_res.success);
    assert(!reg_res.token.empty());
    assert(reg_res.user.username == "alice");

    // 2. Duplicate registration rejected
    auto reg_dup = auth_service.register_user("alice", "AnotherPassword");
    assert(!reg_dup.success);

    // 3. Login with correct password
    auto login_res = auth_service.login_user("alice", "SuperSecret123");
    assert(login_res.success);
    assert(!login_res.token.empty());

    // 4. Login with wrong password
    auto login_fail = auth_service.login_user("alice", "WrongPassword");
    assert(!login_fail.success);

    // 5. Authenticate session token
    auto user_opt = auth_service.authenticate_token(login_res.token);
    assert(user_opt.has_value());
    assert(user_opt->username == "alice");

    // 6. Logout
    assert(auth_service.logout_user(login_res.token));
    assert(!auth_service.authenticate_token(login_res.token).has_value());

    cleanup(db_file);
    std::cout << "[PASS] test_user_registration_and_login" << std::endl;
}

int main() {
    std::cout << "Running Auth tests..." << std::endl;
    test_crypto_primitives();
    test_user_registration_and_login();
    std::cout << "All Auth tests passed successfully!" << std::endl;
    return 0;
}
