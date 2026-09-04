#include "server.hpp"
#include "../util/logger.hpp"

#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

namespace pastebin::http {

void Server::init_network() {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        LOG_ERROR("WSAStartup failed with error: " + std::to_string(res));
    }
#endif
}

void Server::cleanup_network() {
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}

void Server::close_socket(socket_t s) {
    if (s != INVALID_SOCKET_HANDLE) {
#if defined(_WIN32) || defined(_WIN64)
        closesocket(s);
#else
        close(s);
#endif
    }
}

Server::Server(int port, std::string host, size_t thread_count)
    : port_(port), host_(std::move(host)), thread_count_(thread_count) {
    init_network();
    not_found_handler_ = [](const Request&) {
        return Response::not_found("404 Not Found");
    };
}

Server::~Server() {
    stop();
    cleanup_network();
}

void Server::get(const std::string& path, Handler handler) {
    route("GET", path, std::move(handler));
}

void Server::post(const std::string& path, Handler handler) {
    route("POST", path, std::move(handler));
}

void Server::del(const std::string& path, Handler handler) {
    route("DELETE", path, std::move(handler));
}

void Server::route(const std::string& method, const std::string& path, Handler handler) {
    Route r;
    r.method = method;
    r.pattern = path;
    r.handler = std::move(handler);
    r.is_prefix = (!path.empty() && path.back() == '*');
    if (r.is_prefix) {
        r.pattern.pop_back(); // Remove trailing '*'
    }
    routes_.push_back(std::move(r));
}

void Server::set_not_found_handler(Handler handler) {
    not_found_handler_ = std::move(handler);
}

bool Server::start() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET_HANDLE) {
        LOG_ERROR("Failed to create socket");
        return false;
    }

    // Set SO_REUSEADDR
    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (host_ == "0.0.0.0" || host_.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);
    }

    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_CODE) {
        LOG_ERROR("Failed to bind socket to " + host_ + ":" + std::to_string(port_));
        close_socket(server_socket_);
        server_socket_ = INVALID_SOCKET_HANDLE;
        return false;
    }

    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR_CODE) {
        LOG_ERROR("Failed to listen on socket");
        close_socket(server_socket_);
        server_socket_ = INVALID_SOCKET_HANDLE;
        return false;
    }

    running_ = true;
    LOG_INFO("HTTP Server listening on http://" + (host_ == "0.0.0.0" ? "localhost" : host_) + ":" + std::to_string(port_));

    while (running_) {
        sockaddr_in client_addr{};
#if defined(_WIN32) || defined(_WIN64)
        int client_addr_len = sizeof(client_addr);
#else
        socklen_t client_addr_len = sizeof(client_addr);
#endif
        socket_t client_sock = accept(server_socket_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET_HANDLE) {
            if (!running_) break;
            LOG_WARN("Failed to accept incoming connection");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        // Spawn a thread to handle client
        std::thread([this, client_sock, ip = std::string(client_ip)]() {
            handle_client(client_sock, ip);
        }).detach();
    }

    return true;
}

void Server::stop() {
    if (running_) {
        running_ = false;
        if (server_socket_ != INVALID_SOCKET_HANDLE) {
            close_socket(server_socket_);
            server_socket_ = INVALID_SOCKET_HANDLE;
        }
        LOG_INFO("HTTP Server stopped");
    }
}

void Server::handle_client(socket_t client_sock, std::string client_ip) {
    LOG_DEBUG("Handling connection from client: " + client_ip);
    std::string buffer;
    char chunk[4096];
    size_t expected_content_length = 0;
    bool headers_parsed = false;
    size_t header_end_pos = std::string::npos;

    // Timeout settings (5 seconds)
#if defined(_WIN32) || defined(_WIN64)
    DWORD timeout = 5000;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    while (true) {
        int bytes_read = recv(client_sock, chunk, sizeof(chunk), 0);
        if (bytes_read <= 0) {
            break;
        }

        buffer.append(chunk, bytes_read);

        if (!headers_parsed) {
            header_end_pos = buffer.find("\r\n\r\n");
            size_t delim_len = 4;
            if (header_end_pos == std::string::npos) {
                header_end_pos = buffer.find("\n\n");
                delim_len = 2;
            }

            if (header_end_pos != std::string::npos) {
                headers_parsed = true;
                // Look for Content-Length in header section
                std::string header_sec = buffer.substr(0, header_end_pos);
                std::string cl_key = "content-length:";
                std::string lower_header = header_sec;
                std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                size_t cl_pos = lower_header.find(cl_key);
                if (cl_pos != std::string::npos) {
                    size_t val_start = cl_pos + cl_key.size();
                    size_t line_end = header_sec.find_first_of("\r\n", val_start);
                    std::string cl_str = header_sec.substr(val_start, line_end - val_start);
                    try {
                        expected_content_length = std::stoull(cl_str);
                    } catch (...) {
                        expected_content_length = 0;
                    }
                }

                size_t body_bytes_so_far = buffer.size() - (header_end_pos + delim_len);
                if (body_bytes_so_far >= expected_content_length) {
                    break; // Complete request read
                }
            }
        } else {
            size_t delim_len = (buffer.find("\r\n\r\n") != std::string::npos) ? 4 : 2;
            size_t body_bytes_so_far = buffer.size() - (header_end_pos + delim_len);
            if (body_bytes_so_far >= expected_content_length) {
                break; // Complete request read
            }
        }
    }

    if (!buffer.empty()) {
        auto req_opt = Request::parse(buffer);
        Response res;

        if (req_opt) {
            const auto& req = *req_opt;
            if (req.method() == "OPTIONS") {
                // Handle CORS preflight
                res.status(StatusCode::NO_CONTENT);
            } else {
                res = dispatch(req);
            }
        } else {
            res = Response::bad_request("Malformed HTTP Request");
        }

        std::string res_str = res.to_string();
        send(client_sock, res_str.data(), static_cast<int>(res_str.size()), 0);
    }

    close_socket(client_sock);
}

Response Server::dispatch(const Request& req) {
    LOG_DEBUG(req.method() + " " + req.path());

    // 1. Check exact match
    for (const auto& route : routes_) {
        if (route.method == req.method() && !route.is_prefix && route.pattern == req.path()) {
            return route.handler(req);
        }
    }

    // 2. Check prefix / parameterized routes
    for (const auto& route : routes_) {
        if (route.method == req.method() && route.is_prefix) {
            if (req.path().rfind(route.pattern, 0) == 0) { // starts_with
                return route.handler(req);
            }
        }
    }

    // 3. Fallback to not_found_handler
    return not_found_handler_(req);
}

} // namespace pastebin::http
