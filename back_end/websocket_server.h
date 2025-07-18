#pragma once

#include "websocket_session.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WebSocketSession;

class WebSocketServerNotificationClient
{
public:
    virtual const std::string& getName() const = 0;
    virtual bool handleWebSocketValueRequest() const = 0;
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
    void close_all_sessions();
    void end_session(const std::string& session_id);

    // Broadcast message to all sessions
    void broadcast(std::shared_ptr<WebSocketMessage> webSocketMessage);

    // Internal: session management called by sessions
    bool registerSession(std::shared_ptr<WebSocketSession> session);
    bool unregisterSession(std::shared_ptr<WebSocketSession> session);

    //Signal management
    void broadcast_signal_to_websocket(const std::string& signal_name, std::shared_ptr<WebSocketMessage> webSocketMessage);
    void subscribe_session_to_signal(const std::string& session_id, const std::string& signal_name);
    void unsubscribe_session_from_signal(const std::string& session_id, const std::string& signal_name);
    void unsubscribe_session_from_all_signals(const std::string& session_id);    

    void register_notification_client(const std::string& client_name, WebSocketServerNotificationClient* client);
    void unregister_notification_client(const std::string& client_name);

    boost::asio::io_context& get_io_context() { return ioc_; }

private:
    void do_accept();
    void handle_accept(beast::error_code ec, tcp::socket socket);

    net::io_context ioc_;
    net::strand<net::io_context::executor_type> strand_;
    tcp::acceptor acceptor_;

    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;

    std::unordered_map<std::string, WebSocketServerNotificationClient*> notification_clients_;
    std::mutex notification_clients_mutex_;

    std::unordered_map<std::string, std::unordered_set<std::string>> signal_subscriptions_;
    std::mutex signal_subscriptions_mutex_;

    std::vector<std::thread> thread_pool_;
    unsigned int thread_count_;

    std::shared_ptr<spdlog::logger> logger_;

    bool running_ = false;
};