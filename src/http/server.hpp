#pragma once

#include "request.hpp"
#include "response.hpp"

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define INVALID_SOCKET_HANDLE INVALID_SOCKET
#define SOCKET_ERROR_CODE SOCKET_ERROR
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET_HANDLE (-1)
#define SOCKET_ERROR_CODE (-1)
#endif

namespace pastebin::http {

using Handler = std::function<Response(const Request&)>;

struct Route {
    std::string method;
    std::string pattern;
    Handler handler;
    bool is_prefix{false};
};

class Server {
public:
    explicit Server(int port = 8080, std::string host = "0.0.0.0", size_t thread_count = 8);
    ~Server();

    // Route registration
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void del(const std::string& path, Handler handler);
    void route(const std::string& method, const std::string& path, Handler handler);
    void set_not_found_handler(Handler handler);

    // Lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_; }

    int port() const { return port_; }
    const std::string& host() const { return host_; }

private:
    int port_;
    std::string host_;
    size_t thread_count_;
    std::atomic<bool> running_{false};
    socket_t server_socket_{INVALID_SOCKET_HANDLE};

    std::vector<Route> routes_;
    Handler not_found_handler_;

    void handle_client(socket_t client_sock, std::string client_ip);
    Response dispatch(const Request& req);
    static void close_socket(socket_t s);
    static void init_network();
    static void cleanup_network();
};

} // namespace pastebin::http
