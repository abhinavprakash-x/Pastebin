#include "util/logger.hpp"
#include "util/key_generator.hpp"
#include "db/database.hpp"
#include "auth/auth_service.hpp"
#include "storage/file_storage.hpp"
#include "paste/paste_service.hpp"
#include "http/server.hpp"
#include "routes/routes.hpp"

#include <iostream>
#include <string>
#include <filesystem>
#include <csignal>
#include <memory>

static std::shared_ptr<pastebin::http::Server> g_server;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[INFO] Graceful shutdown initiated..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
    }
}

void print_help(const char* prog_name) {
    std::cout << "Pastebin - Lightweight C++ Paste Service with Auth & Encryption\n\n"
              << "Usage: " << prog_name << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>       Port to listen on (default: 8080)\n"
              << "  -h, --host <host>       Host address to bind to (default: 0.0.0.0)\n"
              << "  -d, --data-dir <dir>    Directory for paste storage (default: data)\n"
              << "  -w, --web-dir <dir>     Directory for web static assets (default: web)\n"
              << "  --db <path>             Path to SQLite database (default: data/pastebin.db)\n"
              << "  -s, --shard             Enable directory sharding for pastes\n"
              << "  --debug                 Enable debug logging output\n"
              << "  --help                  Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string host = "0.0.0.0";
    std::filesystem::path data_dir = "data";
    std::filesystem::path web_dir = "web";
    std::filesystem::path db_path = "data/pastebin.db";
    bool enable_sharding = false;
    bool debug_log = false;

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::stoi(argv[++i]);
            }
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            }
        } else if (arg == "-d" || arg == "--data-dir") {
            if (i + 1 < argc) {
                data_dir = argv[++i];
            }
        } else if (arg == "-w" || arg == "--web-dir") {
            if (i + 1 < argc) {
                web_dir = argv[++i];
            }
        } else if (arg == "--db") {
            if (i + 1 < argc) {
                db_path = argv[++i];
            }
        } else if (arg == "-s" || arg == "--shard") {
            enable_sharding = true;
        } else if (arg == "--debug") {
            debug_log = true;
        } else if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
    }

    if (debug_log) {
        pastebin::util::Logger::instance().set_level(pastebin::util::LogLevel::DEBUG_LVL);
    }

    LOG_INFO("=================================================");
    LOG_INFO(" Starting Pastebin C++ Service (Auth & Crypto)   ");
    LOG_INFO("=================================================");
    LOG_INFO("Port:        " + std::to_string(port));
    LOG_INFO("Host:        " + host);
    LOG_INFO("Data dir:    " + data_dir.string());
    LOG_INFO("Web dir:     " + web_dir.string());
    LOG_INFO("Database:    " + db_path.string());
    LOG_INFO("Sharding:    " + std::string(enable_sharding ? "enabled" : "disabled"));

    // Check if web directory exists
    std::error_code ec;
    if (!std::filesystem::exists(web_dir, ec)) {
        if (std::filesystem::exists("../web", ec)) {
            web_dir = "../web";
        }
    }

    // Initialize Database
    auto database = std::make_shared<pastebin::db::Database>(db_path);
    if (!database->initialize()) {
        LOG_ERROR("Failed to initialize SQLite database");
        return 1;
    }

    // Initialize Auth Service
    auto auth_service = std::make_shared<pastebin::auth::AuthService>(database);

    // Initialize core components
    auto key_generator = std::make_shared<pastebin::util::KeyGenerator>(6);
    auto storage = std::make_shared<pastebin::storage::FileStorage>(data_dir, enable_sharding);
    auto paste_service = std::make_shared<pastebin::paste::PasteService>(storage, database, key_generator);

    // Initialize HTTP server
    g_server = std::make_shared<pastebin::http::Server>(port, host);
    pastebin::routes::RouteManager route_mgr(*g_server, paste_service, auth_service, web_dir);
    route_mgr.register_routes();

    // Register signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "\nPastebin running at http://" << (host == "0.0.0.0" ? "localhost" : host)
              << ":" << port << "\nPress Ctrl+C to stop.\n\n" << std::flush;

    if (!g_server->start()) {
        LOG_ERROR("Server failed to start");
        return 1;
    }

    LOG_INFO("Server terminated gracefully.");
    return 0;
}
