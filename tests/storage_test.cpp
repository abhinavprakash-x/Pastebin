#include "../src/storage/file_storage.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

void cleanup_test_dir(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

void test_save_and_load() {
    std::filesystem::path test_dir = "test_data_save_load";
    cleanup_test_dir(test_dir);

    pastebin::storage::FileStorage storage(test_dir, false);
    std::string key = "testKey1";
    std::string content = "Hello, Pastebin World!\nLine 2 with special chars: @#$%^&*()\r\nLine 3";

    assert(storage.save(key, content));
    assert(storage.exists(key));

    auto loaded = storage.load(key);
    assert(loaded.has_value());
    assert(*loaded == content);

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_save_and_load" << std::endl;
}

void test_missing_key() {
    std::filesystem::path test_dir = "test_data_missing";
    cleanup_test_dir(test_dir);

    pastebin::storage::FileStorage storage(test_dir, false);
    assert(!storage.exists("nonExistentKey"));
    assert(!storage.load("nonExistentKey").has_value());

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_missing_key" << std::endl;
}

void test_path_traversal_prevention() {
    std::filesystem::path test_dir = "test_data_traversal";
    cleanup_test_dir(test_dir);

    pastebin::storage::FileStorage storage(test_dir, false);
    // Malicious keys with traversal attempt
    assert(!storage.save("../escape_key", "malicious data"));
    assert(!storage.save("..\\escape_key", "malicious data"));
    assert(!storage.save("/root/key", "malicious data"));
    assert(!storage.load("../escape_key").has_value());
    assert(!storage.load("..\\escape_key").has_value());

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_path_traversal_prevention" << std::endl;
}

void test_sharded_storage() {
    std::filesystem::path test_dir = "test_data_sharded";
    cleanup_test_dir(test_dir);

    pastebin::storage::FileStorage storage(test_dir, true);
    std::string key = "a7K3xQ";
    std::string content = "Sharded paste content";

    assert(storage.save(key, content));
    assert(storage.exists(key));

    auto loaded = storage.load(key);
    assert(loaded.has_value());
    assert(*loaded == content);

    // Verify subdirectories created: test_data_sharded/a7/K3/a7K3xQ.txt
    std::filesystem::path expected_path = test_dir / "a7" / "K3" / (key + ".txt");
    assert(std::filesystem::exists(expected_path));

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_sharded_storage" << std::endl;
}

void test_large_content() {
    std::filesystem::path test_dir = "test_data_large";
    cleanup_test_dir(test_dir);

    pastebin::storage::FileStorage storage(test_dir, false);
    std::string key = "largeKey";
    std::string content(1024 * 1024, 'X'); // 1 MB string

    assert(storage.save(key, content));
    auto loaded = storage.load(key);
    assert(loaded.has_value());
    assert(loaded->size() == content.size());
    assert(*loaded == content);

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_large_content (1MB file)" << std::endl;
}

int main() {
    std::cout << "Running Storage tests..." << std::endl;
    test_save_and_load();
    test_missing_key();
    test_path_traversal_prevention();
    test_sharded_storage();
    test_large_content();
    std::cout << "All Storage tests passed successfully!" << std::endl;
    return 0;
}
