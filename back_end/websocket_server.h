#pragma once

#include "websocket_session.h"

class IWebSocketServer_BackendClient
{
public:
    virtual ~IWebSocketServer_BackendClient() = default;
    virtual const std::string& GetName() const = 0;
    virtual void on_message_received_from_web_socket(const std::string& message) = 0;
};

class WebSocketServer : public MessageTypeHelper, public std::enable_shared_from_this<WebSocketServer>
{
public:
    WebSocketServer(short port);
    ~WebSocketServer();
    void Run();
    void Stop();
    void broadcast_message_to_websocket(const WebSocketMessage& webSocketMessage);
    void broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& webSocketMessage);
    void register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client);
    void deregister_backend_client(const std::string& client_name);
    void close_session(const std::string& session_id);
    void close_all_sessions();
    boost::asio::io_context& get_io_context() { return ioc_; }

private:
    boost::asio::io_context ioc_;
    short port_;
    std::mutex session_mutex_;
    tcp::acceptor acceptor_;
    std::thread ioc_thread_;
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_;
    std::unordered_map<std::string, std::shared_ptr<IWebSocketServer_BackendClient>> backend_clients_;
    std::shared_ptr<spdlog::logger> logger_;

    void register_session(std::shared_ptr<WebSocketSession> session);
    void do_accept();
};
