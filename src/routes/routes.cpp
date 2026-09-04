#include "routes.hpp"
#include "../util/logger.hpp"
#include "../util/key_generator.hpp"

#include <sstream>
#include <iomanip>

namespace pastebin::routes {

RouteManager::RouteManager(
    http::Server& server,
    std::shared_ptr<paste::PasteService> paste_service,
    std::shared_ptr<auth::AuthService> auth_service,
    std::filesystem::path web_dir
) : server_(server),
    paste_service_(std::move(paste_service)),
    auth_service_(std::move(auth_service)),
    web_dir_(std::move(web_dir)) {
}

void RouteManager::register_routes() {
    // Static assets & UI
    server_.get("/", [this](const http::Request& req) { return handle_static_file(req); });
    server_.get("/index.html", [this](const http::Request& req) { return handle_static_file(req); });
    server_.get("/style.css", [this](const http::Request& req) { return handle_static_file(req); });
    server_.get("/app.js", [this](const http::Request& req) { return handle_static_file(req); });
    server_.get("/favicon.ico", [this](const http::Request& req) { return handle_static_file(req); });

    // Health check
    server_.get("/api/health", [this](const http::Request& req) { return handle_health(req); });

    // Auth endpoints
    server_.post("/api/auth/register", [this](const http::Request& req) { return handle_register(req); });
    server_.post("/api/auth/login", [this](const http::Request& req) { return handle_login(req); });
    server_.post("/api/auth/logout", [this](const http::Request& req) { return handle_logout(req); });
    server_.get("/api/auth/me", [this](const http::Request& req) { return handle_me(req); });

    // User pastes & Management
    server_.get("/api/user/pastes", [this](const http::Request& req) { return handle_get_user_pastes(req); });
    server_.del("/api/pastes/*", [this](const http::Request& req) { return handle_delete_paste(req); });

    // Paste endpoints
    server_.post("/api/pastes", [this](const http::Request& req) { return handle_create_paste(req); });
    server_.get("/api/pastes/*", [this](const http::Request& req) { return handle_get_paste(req); });
    server_.get("/raw/*", [this](const http::Request& req) { return handle_get_raw_paste(req); });

    // Fallback 404 handler
    server_.set_not_found_handler([this](const http::Request& req) {
        std::string rel_path = req.path();
        if (!rel_path.empty() && rel_path[0] == '/') {
            rel_path = rel_path.substr(1);
        }
        std::filesystem::path potential_file = web_dir_ / rel_path;
        std::error_code ec;
        if (!rel_path.empty() && std::filesystem::is_regular_file(potential_file, ec) && !ec) {
            return http::Response::file(potential_file);
        }
        return http::Response::not_found("404 Not Found");
    });
}

std::optional<auth::User> RouteManager::extract_authenticated_user(const http::Request& req) {
    if (!auth_service_) return std::nullopt;
    auto token_opt = req.get_bearer_token();
    if (!token_opt) return std::nullopt;
    return auth_service_->authenticate_token(*token_opt);
}

std::string RouteManager::escape_json(const std::string& str) {
    std::ostringstream o;
    for (char c : str) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b";  break;
            case '\f': o << "\\f";  break;
            case '\n': o << "\\n";  break;
            case '\r': o << "\\r";  break;
            case '\t': o << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1f) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

std::string RouteManager::extract_json_field(const std::string& json_str, const std::string& field_name) {
    std::string needle = "\"" + field_name + "\"";
    size_t pos = json_str.find(needle);
    if (pos == std::string::npos) return "";

    pos += needle.size();
    size_t colon = json_str.find(':', pos);
    if (colon == std::string::npos) return "";

    size_t start_quote = json_str.find('"', colon + 1);
    if (start_quote == std::string::npos) return "";

    std::string val;
    bool escaped = false;
    for (size_t i = start_quote + 1; i < json_str.size(); ++i) {
        char c = json_str[i];
        if (escaped) {
            switch (c) {
                case '"': val.push_back('"'); break;
                case '\\': val.push_back('\\'); break;
                case '/': val.push_back('/'); break;
                case 'b': val.push_back('\b'); break;
                case 'f': val.push_back('\f'); break;
                case 'n': val.push_back('\n'); break;
                case 'r': val.push_back('\r'); break;
                case 't': val.push_back('\t'); break;
                default: val.push_back(c); break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break;
        } else {
            val.push_back(c);
        }
    }
    return val;
}

bool RouteManager::extract_json_bool(const std::string& json_str, const std::string& field_name, bool default_val) {
    std::string needle = "\"" + field_name + "\"";
    size_t pos = json_str.find(needle);
    if (pos == std::string::npos) return default_val;

    pos += needle.size();
    size_t colon = json_str.find(':', pos);
    if (colon == std::string::npos) return default_val;

    size_t val_pos = colon + 1;
    while (val_pos < json_str.size() && std::isspace(static_cast<unsigned char>(json_str[val_pos]))) val_pos++;

    if (json_str.rfind("true", val_pos) == val_pos) return true;
    if (json_str.rfind("false", val_pos) == val_pos) return false;
    if (json_str.rfind("1", val_pos) == val_pos) return true;
    if (json_str.rfind("0", val_pos) == val_pos) return false;

    return default_val;
}

std::string RouteManager::extract_key_from_path(const std::string& path, const std::string& prefix) {
    if (path.rfind(prefix, 0) == 0) {
        std::string key = path.substr(prefix.size());
        if (!key.empty() && key.back() == '/') {
            key.pop_back();
        }
        return key;
    }
    return "";
}

// ==========================================
// Authentication Handlers
// ==========================================
http::Response RouteManager::handle_register(const http::Request& req) {
    if (!auth_service_) {
        return http::Response::internal_error("{\"error\": \"Auth service not available\"}").content_type("application/json");
    }

    std::string username = extract_json_field(req.body(), "username");
    std::string password = extract_json_field(req.body(), "password");

    auto result = auth_service_->register_user(username, password);
    if (!result.success) {
        return http::Response::bad_request("{\"error\": \"" + escape_json(result.error_message) + "\"}")
            .content_type("application/json");
    }

    std::string json = "{\n  \"token\": \"" + result.token + "\",\n  \"user\": {\n    \"id\": "
                       + std::to_string(result.user.id) + ",\n    \"username\": \"" + escape_json(result.user.username)
                       + "\"\n  }\n}\n";
    auto res = http::Response::ok_json(json);
    res.status(http::StatusCode::CREATED);
    return res;
}

http::Response RouteManager::handle_login(const http::Request& req) {
    if (!auth_service_) {
        return http::Response::internal_error("{\"error\": \"Auth service not available\"}").content_type("application/json");
    }

    std::string username = extract_json_field(req.body(), "username");
    std::string password = extract_json_field(req.body(), "password");

    auto result = auth_service_->login_user(username, password);
    if (!result.success) {
        auto res = http::Response(http::StatusCode::BAD_REQUEST);
        res.content_type("application/json");
        res.body("{\"error\": \"" + escape_json(result.error_message) + "\"}");
        return res;
    }

    std::string json = "{\n  \"token\": \"" + result.token + "\",\n  \"user\": {\n    \"id\": "
                       + std::to_string(result.user.id) + ",\n    \"username\": \"" + escape_json(result.user.username)
                       + "\"\n  }\n}\n";
    return http::Response::ok_json(json);
}

http::Response RouteManager::handle_logout(const http::Request& req) {
    if (!auth_service_) {
        return http::Response::ok_json("{\"status\": \"ok\"}");
    }

    auto token = req.get_bearer_token();
    if (token) {
        auth_service_->logout_user(*token);
    }

    return http::Response::ok_json("{\"status\": \"logged_out\"}");
}

http::Response RouteManager::handle_me(const http::Request& req) {
    auto user_opt = extract_authenticated_user(req);
    if (!user_opt) {
        auto res = http::Response(http::StatusCode::FORBIDDEN);
        res.content_type("application/json");
        res.body("{\"error\": \"Not authenticated\"}");
        return res;
    }

    std::string json = "{\n  \"id\": " + std::to_string(user_opt->id)
                       + ",\n  \"username\": \"" + escape_json(user_opt->username)
                       + "\",\n  \"created_at\": \"" + escape_json(user_opt->created_at) + "\"\n}\n";
    return http::Response::ok_json(json);
}

// ==========================================
// Paste Operations
// ==========================================
http::Response RouteManager::handle_create_paste(const http::Request& req) {
    std::string content;
    std::string title;
    bool is_public = true;
    bool is_encrypted = false;

    int64_t user_id = 0;
    auto user_opt = extract_authenticated_user(req);
    if (user_opt) {
        user_id = user_opt->id;
    }

    auto ct = req.get_header("Content-Type");
    if (ct && ct->find("application/json") != std::string::npos) {
        content = extract_json_field(req.body(), "content");
        title = extract_json_field(req.body(), "title");
        is_public = extract_json_bool(req.body(), "is_public", true);
        is_encrypted = extract_json_bool(req.body(), "is_encrypted", false);

        if (content.empty() && req.body().find("\"content\"") == std::string::npos) {
            content = req.body();
        }
    } else {
        content = req.body();
    }

    if (content.empty()) {
        return http::Response::bad_request("{\"error\": \"Paste content cannot be empty\"}")
            .content_type("application/json");
    }

    if (content.size() > paste_service_->get_max_paste_size()) {
        return http::Response::payload_too_large("{\"error\": \"Paste size exceeds limit\"}")
            .content_type("application/json");
    }

    // Anonymous pastes must be public
    if (user_id == 0) {
        is_public = true;
    }

    auto key_opt = paste_service_->create_paste(content, user_id, is_public, is_encrypted, title);
    if (!key_opt) {
        return http::Response::internal_error("{\"error\": \"Failed to create paste\"}")
            .content_type("application/json");
    }

    std::string json_response = "{\n  \"key\": \"" + *key_opt + "\",\n  \"is_public\": "
                                + (is_public ? "true" : "false")
                                + ",\n  \"is_encrypted\": "
                                + (is_encrypted ? "true" : "false") + "\n}\n";
    auto res = http::Response::ok_json(json_response);
    res.status(http::StatusCode::CREATED);
    return res;
}

http::Response RouteManager::handle_get_paste(const http::Request& req) {
    std::string key = extract_key_from_path(req.path(), "/api/pastes/");
    if (key.empty()) {
        return http::Response::bad_request("Key required in path: /api/pastes/{key}");
    }

    int64_t requesting_user_id = 0;
    auto user_opt = extract_authenticated_user(req);
    if (user_opt) {
        requesting_user_id = user_opt->id;
    }

    auto result = paste_service_->get_paste_with_access(key, requesting_user_id);

    if (result.status == paste::AccessResult::INVALID_KEY) {
        return http::Response::bad_request("Invalid key format");
    }

    if (result.status == paste::AccessResult::FORBIDDEN_PRIVATE) {
        auto res = http::Response::forbidden("{\"error\": \"This paste is private. Only the author can access it.\"}");
        res.content_type("application/json");
        return res;
    }

    if (result.status == paste::AccessResult::NOT_FOUND) {
        return http::Response::not_found("Paste not found for key: " + key);
    }

    auto accept = req.get_header("Accept");
    if ((accept && accept->find("application/json") != std::string::npos) || result.metadata.is_encrypted) {
        std::string json = "{\n  \"key\": \"" + key
                         + "\",\n  \"content\": \"" + escape_json(result.content)
                         + "\",\n  \"is_public\": " + (result.metadata.is_public ? "true" : "false")
                         + ",\n  \"is_encrypted\": " + (result.metadata.is_encrypted ? "true" : "false")
                         + ",\n  \"title\": \"" + escape_json(result.metadata.title)
                         + "\",\n  \"created_at\": \"" + escape_json(result.metadata.created_at) + "\"\n}\n";
        return http::Response::ok_json(json);
    }

    return http::Response::ok_plain(result.content);
}

http::Response RouteManager::handle_get_raw_paste(const http::Request& req) {
    std::string key = extract_key_from_path(req.path(), "/raw/");
    if (key.empty()) {
        return http::Response::bad_request("Key required in path: /raw/{key}");
    }

    int64_t requesting_user_id = 0;
    auto user_opt = extract_authenticated_user(req);
    if (user_opt) {
        requesting_user_id = user_opt->id;
    }

    auto result = paste_service_->get_paste_with_access(key, requesting_user_id);

    if (result.status == paste::AccessResult::FORBIDDEN_PRIVATE) {
        return http::Response::forbidden("Access Denied: Private paste");
    }

    if (result.status != paste::AccessResult::ALLOWED) {
        return http::Response::not_found("Paste not found for key: " + key);
    }

    return http::Response::ok_plain(result.content);
}

http::Response RouteManager::handle_get_user_pastes(const http::Request& req) {
    auto user_opt = extract_authenticated_user(req);
    if (!user_opt) {
        auto res = http::Response(http::StatusCode::FORBIDDEN);
        res.content_type("application/json");
        res.body("{\"error\": \"Authentication required\"}");
        return res;
    }

    auto pastes = paste_service_->get_user_pastes(user_opt->id);

    std::ostringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < pastes.size(); ++i) {
        const auto& p = pastes[i];
        ss << "  {\n"
           << "    \"key\": \"" << p.key << "\",\n"
           << "    \"title\": \"" << escape_json(p.title) << "\",\n"
           << "    \"is_public\": " << (p.is_public ? "true" : "false") << ",\n"
           << "    \"is_encrypted\": " << (p.is_encrypted ? "true" : "false") << ",\n"
           << "    \"created_at\": \"" << escape_json(p.created_at) << "\",\n"
           << "    \"snippet\": \"" << escape_json(p.snippet) << "\"\n"
           << "  }" << (i + 1 < pastes.size() ? ",\n" : "\n");
    }
    ss << "]\n";

    return http::Response::ok_json(ss.str());
}

http::Response RouteManager::handle_delete_paste(const http::Request& req) {
    auto user_opt = extract_authenticated_user(req);
    if (!user_opt) {
        auto res = http::Response(http::StatusCode::FORBIDDEN);
        res.content_type("application/json");
        res.body("{\"error\": \"Authentication required\"}");
        return res;
    }

    std::string key = extract_key_from_path(req.path(), "/api/pastes/");
    if (key.empty()) {
        return http::Response::bad_request("Key required in path");
    }

    if (!paste_service_->delete_paste(key, user_opt->id)) {
        return http::Response::not_found("{\"error\": \"Paste not found or you do not have permission to delete it\"}")
            .content_type("application/json");
    }

    return http::Response::ok_json("{\"status\": \"deleted\", \"key\": \"" + key + "\"}");
}

http::Response RouteManager::handle_static_file(const http::Request& req) {
    std::string path = req.path();
    if (path == "/" || path.empty()) {
        path = "/index.html";
    }

    if (path[0] == '/') {
        path = path.substr(1);
    }

    if (path.find("..") != std::string::npos) {
        return http::Response::forbidden("Access Denied");
    }

    std::filesystem::path full_path = web_dir_ / path;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
        return http::Response::not_found("File Not Found");
    }

    return http::Response::file(full_path);
}

http::Response RouteManager::handle_health(const http::Request&) {
    return http::Response::ok_json("{\n  \"status\": \"ok\",\n  \"service\": \"pastebin-cpp\",\n  \"auth\": true\n}\n");
}

} // namespace pastebin::routes
