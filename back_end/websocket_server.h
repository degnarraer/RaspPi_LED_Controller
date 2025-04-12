#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "logger.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WebSocketServer;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
public:
    explicit WebSocketSession(tcp::socket socket, WebSocketServer& server);
    void run();
    void close();
    void send_message(const std::string& message);
    std::string GetSessionID() const;

private:
    void do_read();
    void on_read(std::size_t bytes_transferred);
    void do_write();

    websocket::stream<beast::tcp_stream> ws_;
    WebSocketServer& server_;
    std::shared_ptr<spdlog::logger> logger_;
    beast::flat_buffer buffer_;
    std::string session_id_;

    std::deque<std::string> outgoing_messages_;
    bool writing_ = false;
    std::mutex write_mutex_;
};

class IWebSocketServer_BackendClient
{
public:
    virtual ~IWebSocketServer_BackendClient() = default;
    virtual const std::string& GetName() const = 0;
    virtual void on_message_received_from_web_socket(const std::string& message) = 0;
};

class WebSocketServer
{
public:
    WebSocketServer(short port);
    ~WebSocketServer();
    void Run();
    void Stop();
    void broadcast_message_to_websocket(const std::string& message);
    void register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client);
    void deregister_backend_client(const std::string& client_name);
    void close_session(const std::string& session_id);

private:
    boost::asio::io_context ioc_;
    short port_;
    tcp::acceptor acceptor_;
    std::thread ioc_thread_;
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_;
    std::unordered_map<std::string, std::shared_ptr<IWebSocketServer_BackendClient>> backend_clients_;
    std::shared_ptr<spdlog::logger> logger_;

    void register_session(std::shared_ptr<WebSocketSession> session);
    void do_accept();
};
