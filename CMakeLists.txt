cmake_minimum_required(VERSION 3.15)
project(Pastebin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if (MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Include directories
include_directories(src third_party/sqlite3)

# Core sources
set(PASTEBIN_CORE_SOURCES
    src/util/logger.cpp
    src/util/key_generator.cpp
    src/util/crypto.cpp
    src/storage/file_storage.cpp
    src/paste/paste_service.cpp
    src/db/database.cpp
    src/auth/auth_service.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/server.cpp
    src/routes/routes.cpp
    third_party/sqlite3/sqlite3.c
)

# Main executable
add_executable(pastebin
    src/main.cpp
    ${PASTEBIN_CORE_SOURCES}
)

if (WIN32)
    target_link_libraries(pastebin PRIVATE ws2_32)
endif()

# Testing
enable_testing()

add_executable(key_generator_test
    tests/key_generator_test.cpp
    src/util/key_generator.cpp
)
add_test(NAME KeyGeneratorTest COMMAND key_generator_test)

add_executable(storage_test
    tests/storage_test.cpp
    src/storage/file_storage.cpp
    src/util/key_generator.cpp
    src/util/logger.cpp
)
add_test(NAME StorageTest COMMAND storage_test)

add_executable(paste_service_test
    tests/paste_service_test.cpp
    src/paste/paste_service.cpp
    src/storage/file_storage.cpp
    src/db/database.cpp
    src/util/key_generator.cpp
    src/util/logger.cpp
    third_party/sqlite3/sqlite3.c
)
add_test(NAME PasteServiceTest COMMAND paste_service_test)

add_executable(auth_test
    tests/auth_test.cpp
    src/auth/auth_service.cpp
    src/db/database.cpp
    src/util/crypto.cpp
    src/util/logger.cpp
    third_party/sqlite3/sqlite3.c
)
add_test(NAME AuthTest COMMAND auth_test)

add_executable(access_control_test
    tests/access_control_test.cpp
    src/paste/paste_service.cpp
    src/storage/file_storage.cpp
    src/db/database.cpp
    src/util/key_generator.cpp
    src/util/logger.cpp
    third_party/sqlite3/sqlite3.c
)
add_test(NAME AccessControlTest COMMAND access_control_test)
