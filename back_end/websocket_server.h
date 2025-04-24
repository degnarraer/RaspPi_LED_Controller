#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WebSocketServer;

class MessageTypeHelper
{
public:
    enum class MessageType
    {
        Subscribe,
        Unsubscribe,
        Text,
        Signal,
        Echo,
        Unknown
    };

    static MessageType FromString(const std::string& str);
    static std::string ToString(MessageType type);

protected:
    static const std::unordered_map<std::string, MessageType> string_to_type_;
    static const std::unordered_map<MessageType, std::string> type_to_string_;
};

class WebSocketSession : public MessageTypeHelper, public std::enable_shared_from_this<WebSocketSession>
{
public:
    struct RetryMessage
    {
        std::string message;
        int retry_count = 0;
        RetryMessage(std::string msg)
        : message(std::move(msg), 0) {}
    };
    explicit WebSocketSession(tcp::socket socket, WebSocketServer& server);
    void run();
    void close();
    void send_message(const std::string& message);
    std::string GetSessionID() const;

    bool subscribe_to_signal(const std::string& signal_name);
    bool unsubscribe_from_signal(const std::string& signal_name);
    bool is_subscribed_to(const std::string& signal_name) const;

private:
    void do_read();
    void on_read(std::size_t bytes_transferred);
    void handle_read_error(beast::error_code ec);
    void do_write();
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void handle_write_error(beast::error_code ec);
    void maybe_backoff();
    void schedule_backoff();
    void resume_sending();
    void remove_sent_message_from_retry_queue();
    bool is_valid_utf8(const std::string& str);

    websocket::stream<beast::tcp_stream> ws_;
    WebSocketServer& server_;
    std::shared_ptr<spdlog::logger> logger_;
    beast::flat_buffer buffer_;
    std::string session_id_;

    std::deque<std::string> outgoing_messages_;
    bool writing_ = false;
    mutable std::mutex read_mutex_;
    mutable std::mutex write_mutex_;

    std::deque<RetryMessage> retry_messages_;
    std::mutex retry_mutex_;

    const int MAX_RETRY_COUNT = 5;
    const std::chrono::seconds MAX_RETRY_AGE = std::chrono::seconds(5);

    std::unordered_set<std::string> subscribed_signals_;
    mutable std::mutex subscription_mutex_;

    static constexpr size_t MAX_QUEUE_SIZE = 500;
    static constexpr int BASE_BACKOFF_MS = 50;
    int backoff_attempts_ = 0;
    std::atomic<bool> backoff_enabled_{false};
    asio::steady_timer backoff_timer_;
};

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
    void broadcast_message_to_websocket(const std::string& message);
    void broadcast_signal_to_websocket(const std::string& signal_name, const std::string& message);
    void register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client);
    void deregister_backend_client(const std::string& client_name);
    void close_session(const std::string& session_id);
    void close_all_sessions();

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
