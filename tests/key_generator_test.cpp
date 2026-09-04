#include "../src/util/key_generator.hpp"
#include <iostream>
#include <cassert>
#include <unordered_set>

void test_key_length_and_charset() {
    pastebin::util::KeyGenerator gen(6);

    for (int i = 0; i < 100; ++i) {
        std::string key = gen.generate();
        assert(key.length() == 6);
        assert(pastebin::util::KeyGenerator::validate_key(key));
    }

    // Custom length
    std::string key10 = gen.generate(10);
    assert(key10.length() == 10);
    assert(pastebin::util::KeyGenerator::validate_key(key10));

    std::cout << "[PASS] test_key_length_and_charset" << std::endl;
}

void test_key_validation() {
    // Valid keys
    assert(pastebin::util::KeyGenerator::validate_key("a7K3xQ"));
    assert(pastebin::util::KeyGenerator::validate_key("123456"));
    assert(pastebin::util::KeyGenerator::validate_key("abcdef"));
    assert(pastebin::util::KeyGenerator::validate_key("ABCDEF"));
    assert(pastebin::util::KeyGenerator::validate_key("abc"));

    // Invalid keys (path traversal, control chars, symbols, empty, too short/long)
    assert(!pastebin::util::KeyGenerator::validate_key(""));
    assert(!pastebin::util::KeyGenerator::validate_key("ab")); // too short (<3)
    assert(!pastebin::util::KeyGenerator::validate_key("../secret"));
    assert(!pastebin::util::KeyGenerator::validate_key("..\\secret"));
    assert(!pastebin::util::KeyGenerator::validate_key("key/with/slash"));
    assert(!pastebin::util::KeyGenerator::validate_key("key.with.dot"));
    assert(!pastebin::util::KeyGenerator::validate_key("key with space"));
    assert(!pastebin::util::KeyGenerator::validate_key("key@#$"));
    assert(!pastebin::util::KeyGenerator::validate_key(std::string(65, 'a'))); // too long

    std::cout << "[PASS] test_key_validation" << std::endl;
}

void test_collision_rate() {
    pastebin::util::KeyGenerator gen(8);
    std::unordered_set<std::string> seen;
    constexpr int NUM_KEYS = 5000;

    for (int i = 0; i < NUM_KEYS; ++i) {
        std::string key = gen.generate();
        assert(seen.find(key) == seen.end()); // No duplicates in 5000 iterations of 8-char keys
        seen.insert(key);
    }

    std::cout << "[PASS] test_collision_rate (5000 keys without collision)" << std::endl;
}

int main() {
    std::cout << "Running KeyGenerator tests..." << std::endl;
    test_key_length_and_charset();
    test_key_validation();
    test_collision_rate();
    std::cout << "All KeyGenerator tests passed successfully!" << std::endl;
    return 0;
}
