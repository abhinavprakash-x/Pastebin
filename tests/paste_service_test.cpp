#include "../src/paste/paste_service.hpp"
#include "../src/storage/file_storage.hpp"
#include "../src/db/database.hpp"
#include "../src/util/key_generator.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <memory>

void cleanup_test_dir(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

void test_create_and_retrieve_paste() {
    std::filesystem::path test_dir = "test_service_data";
    cleanup_test_dir(test_dir);

    auto storage = std::make_shared<pastebin::storage::FileStorage>(test_dir, false);
    auto key_gen = std::make_shared<pastebin::util::KeyGenerator>(6);
    pastebin::paste::PasteService service(storage, nullptr, key_gen);

    std::string sample_paste = "Console.WriteLine(\"Hello, World!\");";
    auto key_opt = service.create_paste(sample_paste);

    assert(key_opt.has_value());
    assert(key_opt->length() == 6);
    assert(service.exists(*key_opt));

    auto retrieved = service.get_paste(*key_opt);
    assert(retrieved.has_value());
    assert(*retrieved == sample_paste);

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_create_and_retrieve_paste" << std::endl;
}

void test_empty_paste_rejected() {
    std::filesystem::path test_dir = "test_service_empty";
    cleanup_test_dir(test_dir);

    auto storage = std::make_shared<pastebin::storage::FileStorage>(test_dir, false);
    pastebin::paste::PasteService service(storage, nullptr, nullptr);

    auto key_opt = service.create_paste("");
    assert(!key_opt.has_value());

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_empty_paste_rejected" << std::endl;
}

void test_max_size_enforced() {
    std::filesystem::path test_dir = "test_service_size";
    cleanup_test_dir(test_dir);

    auto storage = std::make_shared<pastebin::storage::FileStorage>(test_dir, false);
    // Limit to 100 bytes for test
    pastebin::paste::PasteService service(storage, nullptr, nullptr, 100);

    std::string valid_paste(50, 'A');
    std::string too_large_paste(150, 'B');

    assert(service.create_paste(valid_paste).has_value());
    assert(!service.create_paste(too_large_paste).has_value());

    cleanup_test_dir(test_dir);
    std::cout << "[PASS] test_max_size_enforced" << std::endl;
}

int main() {
    std::cout << "Running PasteService tests..." << std::endl;
    test_create_and_retrieve_paste();
    test_empty_paste_rejected();
    test_max_size_enforced();
    std::cout << "All PasteService tests passed successfully!" << std::endl;
    return 0;
}
