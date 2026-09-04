#include "response.hpp"
#include <sstream>
#include <fstream>
#include <system_error>

namespace pastebin::http {

Response::Response(StatusCode status) : status_(status) {
    headers_["Server"] = "Pastebin-CPP/1.0";
    headers_["Connection"] = "close";
    headers_["Access-Control-Allow-Origin"] = "*";
    headers_["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
    headers_["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Auth-Token";
}

Response Response::ok_html(const std::string& html) {
    Response res(StatusCode::OK);
    res.content_type("text/html; charset=utf-8");
    res.body(html);
    return res;
}

Response Response::ok_json(const std::string& json_str) {
    Response res(StatusCode::OK);
    res.content_type("application/json; charset=utf-8");
    res.body(json_str);
    return res;
}

Response Response::ok_plain(const std::string& text) {
    Response res(StatusCode::OK);
    res.content_type("text/plain; charset=utf-8");
    res.body(text);
    return res;
}

Response Response::not_found(const std::string& msg) {
    Response res(StatusCode::NOT_FOUND);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::forbidden(const std::string& msg) {
    Response res(StatusCode::FORBIDDEN);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::bad_request(const std::string& msg) {
    Response res(StatusCode::BAD_REQUEST);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::method_not_allowed(const std::string& msg) {
    Response res(StatusCode::METHOD_NOT_ALLOWED);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::payload_too_large(const std::string& msg) {
    Response res(StatusCode::PAYLOAD_TOO_LARGE);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::internal_error(const std::string& msg) {
    Response res(StatusCode::INTERNAL_SERVER_ERROR);
    res.content_type("text/plain; charset=utf-8");
    res.body(msg);
    return res;
}

Response Response::redirect(const std::string& location) {
    Response res(StatusCode::FOUND);
    res.header("Location", location);
    res.body("");
    return res;
}

Response Response::file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return not_found("File Not Found");
    }

    std::ifstream ifs(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        return internal_error("Failed to read file");
    }

    auto size = ifs.tellg();
    if (size < 0) {
        return internal_error("Failed to determine file size");
    }

    std::string content;
    content.resize(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    if (size > 0) {
        ifs.read(&content[0], size);
    }

    Response res(StatusCode::OK);
    res.content_type(detect_mime_type(path));
    res.body(std::move(content));
    return res;
}

Response& Response::status(StatusCode code) {
    status_ = code;
    return *this;
}

Response& Response::header(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

Response& Response::content_type(const std::string& type) {
    headers_["Content-Type"] = type;
    return *this;
}

Response& Response::body(std::string b) {
    body_ = std::move(b);
    headers_["Content-Length"] = std::to_string(body_.size());
    return *this;
}

std::string Response::to_string() const {
    std::ostringstream ss;
    int code_val = static_cast<int>(status_);
    ss << "HTTP/1.1 " << code_val << " " << status_message(status_) << "\r\n";

    for (const auto& [k, v] : headers_) {
        ss << k << ": " << v << "\r\n";
    }

    ss << "\r\n";
    ss << body_;
    return ss.str();
}

const char* Response::status_message(StatusCode code) {
    switch (code) {
        case StatusCode::OK:                    return "OK";
        case StatusCode::CREATED:               return "Created";
        case StatusCode::NO_CONTENT:            return "No Content";
        case StatusCode::MOVED_PERMANENTLY:     return "Moved Permanently";
        case StatusCode::FOUND:                 return "Found";
        case StatusCode::BAD_REQUEST:           return "Bad Request";
        case StatusCode::FORBIDDEN:             return "Forbidden";
        case StatusCode::NOT_FOUND:             return "Not Found";
        case StatusCode::METHOD_NOT_ALLOWED:    return "Method Not Allowed";
        case StatusCode::PAYLOAD_TOO_LARGE:     return "Payload Too Large";
        case StatusCode::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case StatusCode::NOT_IMPLEMENTED:       return "Not Implemented";
        default:                                return "Unknown";
    }
}

std::string Response::detect_mime_type(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    // Convert to lowercase
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")                    return "text/css; charset=utf-8";
    if (ext == ".js" || ext == ".mjs")    return "application/javascript; charset=utf-8";
    if (ext == ".json")                   return "application/json; charset=utf-8";
    if (ext == ".png")                    return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")  return "image/jpeg";
    if (ext == ".gif")                    return "image/gif";
    if (ext == ".svg")                    return "image/svg+xml";
    if (ext == ".ico")                    return "image/x-icon";
    if (ext == ".txt")                    return "text/plain; charset=utf-8";
    if (ext == ".woff2")                  return "font/woff2";
    if (ext == ".woff")                   return "font/woff";
    if (ext == ".ttf")                    return "font/ttf";

    return "application/octet-stream";
}

} // namespace pastebin::http
