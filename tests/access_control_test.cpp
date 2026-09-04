#include "../src/paste/paste_service.hpp"
#include "../src/storage/file_storage.hpp"
#include "../src/db/database.hpp"
#include "../src/util/key_generator.hpp"

#include <iostream>
#include <cassert>
#include <filesystem>

void cleanup_all(const std::filesystem::path& dir, const std::filesystem::path& db) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::remove(db, ec);
}

void test_public_vs_private_access_control() {
    std::filesystem::path storage_dir = "test_ac_storage";
    std::filesystem::path db_file = "test_ac.db";
    cleanup_all(storage_dir, db_file);

    auto db = std::make_shared<pastebin::db::Database>(db_file);
    assert(db->initialize());

    auto storage = std::make_shared<pastebin::storage::FileStorage>(storage_dir, false);
    auto key_gen = std::make_shared<pastebin::util::KeyGenerator>(6);
    pastebin::paste::PasteService service(storage, db, key_gen);

    int64_t user_alice = 101;
    int64_t user_bob = 102;
    int64_t guest = 0;

    // 1. Alice creates a PUBLIC paste
    auto pub_key = service.create_paste("Alice's Public Code", user_alice, true, false, "Public Snippet");
    assert(pub_key.has_value());

    // Alice, Bob, and Guest can all access the public paste
    assert(service.get_paste_with_access(*pub_key, user_alice).status == pastebin::paste::AccessResult::ALLOWED);
    assert(service.get_paste_with_access(*pub_key, user_bob).status == pastebin::paste::AccessResult::ALLOWED);
    assert(service.get_paste_with_access(*pub_key, guest).status == pastebin::paste::AccessResult::ALLOWED);

    // 2. Alice creates a PRIVATE paste
    auto priv_key = service.create_paste("Alice's Secret Keys", user_alice, false, false, "Private Note");
    assert(priv_key.has_value());

    // Alice (the owner) CAN access it
    auto alice_access = service.get_paste_with_access(*priv_key, user_alice);
    assert(alice_access.status == pastebin::paste::AccessResult::ALLOWED);
    assert(alice_access.content == "Alice's Secret Keys");

    // Bob (another user) is FORBIDDEN
    auto bob_access = service.get_paste_with_access(*priv_key, user_bob);
    assert(bob_access.status == pastebin::paste::AccessResult::FORBIDDEN_PRIVATE);

    // Guest (unauthenticated) is FORBIDDEN
    auto guest_access = service.get_paste_with_access(*priv_key, guest);
    assert(guest_access.status == pastebin::paste::AccessResult::FORBIDDEN_PRIVATE);

    // 3. User Pastes List
    auto alice_pastes = service.get_user_pastes(user_alice);
    assert(alice_pastes.size() == 2);

    auto bob_pastes = service.get_user_pastes(user_bob);
    assert(bob_pastes.empty());

    // 4. Deletion Permissions
    // Bob cannot delete Alice's paste
    assert(!service.delete_paste(*priv_key, user_bob));
    assert(service.exists(*priv_key));

    // Alice can delete her own paste
    assert(service.delete_paste(*priv_key, user_alice));
    assert(!service.exists(*priv_key));

    cleanup_all(storage_dir, db_file);
    std::cout << "[PASS] test_public_vs_private_access_control" << std::endl;
}

int main() {
    std::cout << "Running Access Control tests..." << std::endl;
    test_public_vs_private_access_control();
    std::cout << "All Access Control tests passed successfully!" << std::endl;
    return 0;
}
