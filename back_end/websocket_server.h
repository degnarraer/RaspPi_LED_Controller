#pragma once

#include "websocket_session.h"

class WebSocketServer : public MessageTypeHelper, public std::enable_shared_from_this<WebSocketServer>
{
public:
    WebSocketServer(short port);
    ~WebSocketServer();
    void Run();
    void Stop();
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
