#include "request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace pastebin::http {

static std::string url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex_val = 0;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> hex_val) {
                result.push_back(static_cast<char>(hex_val));
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            result.push_back(' ');
            continue;
        }
        result.push_back(str[i]);
    }
    return result;
}

size_t CaseInsensitiveHash::operator()(const std::string& str) const {
    size_t hash = 0;
    for (char c : str) {
        hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
    }
    return hash;
}

bool CaseInsensitiveEqual::operator()(const std::string& a, const std::string& b) const {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<Request> Request::parse(const std::string& raw_request) {
    if (raw_request.empty()) {
        return std::nullopt;
    }

    Request req;
    size_t header_end = raw_request.find("\r\n\r\n");
    size_t header_delimiter_length = 4;
    if (header_end == std::string::npos) {
        header_end = raw_request.find("\n\n");
        header_delimiter_length = 2;
        if (header_end == std::string::npos) {
            header_end = raw_request.size();
            header_delimiter_length = 0;
        }
    }

    std::string header_section = raw_request.substr(0, header_end);
    std::istringstream stream(header_section);
    std::string request_line;

    if (!std::getline(stream, request_line) || request_line.empty()) {
        return std::nullopt;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    // Parse Request-Line: METHOD URI HTTP-VERSION
    std::istringstream req_line_stream(request_line);
    std::string method, raw_uri, version;
    if (!(req_line_stream >> method >> raw_uri)) {
        return std::nullopt;
    }
    req_line_stream >> version;

    req.method_ = method;
    req.raw_path_ = raw_uri;

    // Split path and query string
    size_t qpos = raw_uri.find('?');
    if (qpos != std::string::npos) {
        req.path_ = url_decode(raw_uri.substr(0, qpos));
        req.query_string_ = raw_uri.substr(qpos + 1);
    } else {
        req.path_ = url_decode(raw_uri);
        req.query_string_ = "";
    }

    // Parse headers
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim leading/trailing whitespace
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();

            req.headers_[key] = value;
        }
    }

    // Parse query params
    req.parse_query();

    // Body
    if (header_delimiter_length > 0 && header_end + header_delimiter_length <= raw_request.size()) {
        req.body_ = raw_request.substr(header_end + header_delimiter_length);
    }

    return req;
}

void Request::parse_query() {
    if (query_string_.empty()) return;

    std::istringstream ss(query_string_);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        if (pair.empty()) continue;
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = url_decode(pair.substr(0, eq));
            std::string value = url_decode(pair.substr(eq + 1));
            query_params_[key] = value;
        } else {
            query_params_[url_decode(pair)] = "";
        }
    }
}

std::optional<std::string> Request::get_header(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> Request::get_query_param(const std::string& name) const {
    auto it = query_params_.find(name);
    if (it != query_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> Request::get_bearer_token() const {
    auto auth_header = get_header("Authorization");
    if (!auth_header) {
        // Also check "X-Auth-Token"
        auto token_header = get_header("X-Auth-Token");
        if (token_header) return *token_header;
        return std::nullopt;
    }

    std::string prefix = "Bearer ";
    if (auth_header->rfind(prefix, 0) == 0) {
        return auth_header->substr(prefix.size());
    }

    // Try without prefix if directly token
    return *auth_header;
}

} // namespace pastebin::http
