#pragma once

#include "../http/server.hpp"
#include "../paste/paste_service.hpp"
#include "../auth/auth_service.hpp"

#include <filesystem>
#include <memory>

namespace pastebin::routes {

class RouteManager {
public:
    RouteManager(
        http::Server& server,
        std::shared_ptr<paste::PasteService> paste_service,
        std::shared_ptr<auth::AuthService> auth_service,
        std::filesystem::path web_dir
    );

    void register_routes();

private:
    http::Server& server_;
    std::shared_ptr<paste::PasteService> paste_service_;
    std::shared_ptr<auth::AuthService> auth_service_;
    std::filesystem::path web_dir_;

    // Handlers
    http::Response handle_register(const http::Request& req);
    http::Response handle_login(const http::Request& req);
    http::Response handle_logout(const http::Request& req);
    http::Response handle_me(const http::Request& req);

    http::Response handle_create_paste(const http::Request& req);
    http::Response handle_get_paste(const http::Request& req);
    http::Response handle_get_raw_paste(const http::Request& req);
    http::Response handle_get_user_pastes(const http::Request& req);
    http::Response handle_delete_paste(const http::Request& req);

    http::Response handle_static_file(const http::Request& req);
    http::Response handle_health(const http::Request& req);

    // Helpers
    std::optional<auth::User> extract_authenticated_user(const http::Request& req);
    static std::string extract_key_from_path(const std::string& path, const std::string& prefix);
    static std::string extract_json_field(const std::string& json_str, const std::string& field_name);
    static bool extract_json_bool(const std::string& json_str, const std::string& field_name, bool default_val = false);
    static std::string escape_json(const std::string& str);
};

} // namespace pastebin::routes
