#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "logger.h"

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class WebSocketServer; // Forward declaration

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
public:
    WebSocketSession(tcp::socket socket, WebSocketServer& server);
    void run();
    void close();
    std::string GetSessionID() const;
    void send_message(const std::string& message);

private:
    WebSocketServer& server_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string session_id_;
    std::shared_ptr<spdlog::logger> logger_;

    void do_read();
    void on_read(std::size_t);
    void do_write(const std::string& message);
};

class IWebSocketServer_BackendClient
{
public:
    virtual ~IWebSocketServer_BackendClient() = default;
    virtual std::string GetName() = 0;
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
