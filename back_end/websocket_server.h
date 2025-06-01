#pragma once

#include "websocket_session.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>

/*
class WebSocketServer : public MessageTypeHelper, public std::enable_shared_from_this<WebSocketServer>
{
public:
    WebSocketServer(short port);
    ~WebSocketServer();
    void run();
    void stop();
    void broadcast_message_to_websocket(const WebSocketMessage& webSocketMessage);
    void broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& webSocketMessage);
    void close_session(const std::string& session_id);
    void end_session(const std::string& session_id);
    void close_all_sessions();
    boost::asio::io_context& get_io_context() { return ioc_; }

private:
    boost::asio::io_context ioc_;
    short port_;
    std::mutex session_mutex_;
    tcp::acceptor acceptor_;
    std::thread ioc_thread_;
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<bool> is_running_ = false;
    std::mutex server_mutex_;

    void register_session(std::shared_ptr<WebSocketSession> session);
    void do_accept();
};
*/

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WebSocketSession;

class WebSocketServerNotificationClient
{
public:
    virtual const std::string& getName() const = 0;
    virtual void onValueRequest() const = 0;
};

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer>
{
public:
    // ctor accepts port and optional thread count (0 = auto detect)
    explicit WebSocketServer(unsigned short port, unsigned int thread_count = 0);

    // Start and stop server
    void start();
    void stop();
    void close_session(const std::string& session_id);
    void end_session(const std::string& session_id);

    // Broadcast message to all sessions
    void broadcast(const std::string& message);

    // Internal: session management called by sessions
    bool registerSession(std::shared_ptr<WebSocketSession> session);
    bool unregisterSession(std::shared_ptr<WebSocketSession> session);

    void register_notification_client(const std::string& client_name, WebSocketServerNotificationClient* client);
    void unregister_notification_client(const std::string& client_name);

    void broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& webSocketMessage);
    void close_all_sessions();
    boost::asio::io_context& get_io_context() { return ioc_; }

private:
    void do_accept();

    net::io_context ioc_;
    tcp::acceptor acceptor_;

    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;

    std::unordered_map<std::string, WebSocketServerNotificationClient*> notification_clients_;
    std::mutex notification_clients_mutex_;

    std::vector<std::thread> thread_pool_;
    unsigned int thread_count_;

    std::shared_ptr<spdlog::logger> logger_;

    bool running_ = false;
};