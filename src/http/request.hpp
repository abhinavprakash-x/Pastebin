#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace pastebin::http {

struct CaseInsensitiveHash {
    size_t operator()(const std::string& str) const;
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const;
};

using HeaderMap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;

class Request {
public:
    Request() = default;

    // Parsing factory
    static std::optional<Request> parse(const std::string& raw_request);

    // Getters
    const std::string& method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::string& raw_path() const { return raw_path_; }
    const std::string& query_string() const { return query_string_; }
    const std::string& body() const { return body_; }
    const HeaderMap& headers() const { return headers_; }

    // Helpers
    std::optional<std::string> get_header(const std::string& name) const;
    std::optional<std::string> get_query_param(const std::string& name) const;
    std::optional<std::string> get_bearer_token() const;

    // Setters
    void set_method(std::string m) { method_ = std::move(m); }
    void set_path(std::string p) { path_ = std::move(p); }
    void set_body(std::string b) { body_ = std::move(b); }
    void set_header(std::string k, std::string v) { headers_[std::move(k)] = std::move(v); }

private:
    std::string method_;
    std::string path_;
    std::string raw_path_;
    std::string query_string_;
    HeaderMap headers_;
    std::unordered_map<std::string, std::string> query_params_;
    std::string body_;

    void parse_query();
};

} // namespace pastebin::http
