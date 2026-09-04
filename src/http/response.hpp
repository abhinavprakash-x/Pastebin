#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

namespace pastebin::http {

enum class StatusCode {
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    BAD_REQUEST = 400,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    PAYLOAD_TOO_LARGE = 413,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501
};

class Response {
public:
    Response(StatusCode status = StatusCode::OK);

    // Factory methods
    static Response ok_html(const std::string& html);
    static Response ok_json(const std::string& json_str);
    static Response ok_plain(const std::string& text);
    static Response not_found(const std::string& msg = "404 Not Found");
    static Response forbidden(const std::string& msg = "403 Forbidden");
    static Response bad_request(const std::string& msg = "400 Bad Request");
    static Response method_not_allowed(const std::string& msg = "405 Method Not Allowed");
    static Response payload_too_large(const std::string& msg = "413 Payload Too Large");
    static Response internal_error(const std::string& msg = "500 Internal Server Error");
    static Response redirect(const std::string& location);

    // File response helper
    static Response file(const std::filesystem::path& path);

    // Content & Headers
    Response& status(StatusCode code);
    Response& header(const std::string& key, const std::string& value);
    Response& content_type(const std::string& type);
    Response& body(std::string b);

    // Getters
    StatusCode status() const { return status_; }
    const std::string& body() const { return body_; }
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }

    // Serialization
    std::string to_string() const;

    static const char* status_message(StatusCode code);
    static std::string detect_mime_type(const std::filesystem::path& path);

private:
    StatusCode status_{StatusCode::OK};
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace pastebin::http
