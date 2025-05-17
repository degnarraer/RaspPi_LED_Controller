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
#include <optional>
#include <boost/locale.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio.hpp>
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

enum MessagePriority
{
    High,
    Medium,
    Low
};

enum WebSocketMessageType
{
    Text,
    Binary
};

struct WebSocketMessage
{
    std::string message;
    std::vector<uint8_t> binary_data;
    WebSocketMessageType webSocket_Message_type = WebSocketMessageType::Text;
    MessagePriority priority = MessagePriority::Low;
    int retry_count = 0;
    bool should_retry = false;
    WebSocketMessage() = default;
    WebSocketMessage(const std::string& msg, MessagePriority p = MessagePriority::Low, bool retry_flag = false)
        : message(msg), priority(p), should_retry(retry_flag) {}
    WebSocketMessage(const std::vector<uint8_t>& data, MessagePriority p = MessagePriority::Low, bool retry_flag = false)
    : binary_data(data), webSocket_Message_type(WebSocketMessageType::Binary), priority(p), should_retry(retry_flag) {}
};

class WebSocketSession : public MessageTypeHelper, public std::enable_shared_from_this<WebSocketSession>
{
public:
    explicit WebSocketSession(tcp::socket socket, WebSocketServer& server);
    void run();
    void close();
    void send_message(const WebSocketMessage& message);
    void send_binary_message(const std::vector<uint8_t>& message);
    std::string GetSessionID() const;

    bool subscribe_to_signal(const std::string& signal_name);
    bool unsubscribe_from_signal(const std::string& signal_name);
    bool is_subscribed_to(const std::string& signal_name) const;

private:
    void do_read();
    void on_read(std::size_t bytes_transferred);
    void handle_subscribe(const json& incoming);
    void handle_unsubscribe(const json& incoming);
    void handle_text_message(const json& incoming);
    void handle_signal_message(const json& incoming);
    void handle_echo_message(const json& incoming);
    void handle_unknown_message(const json& incoming);
    void handleWebSocketError(const std::error_code& ec, const std::string& context = "");
    void do_write();
    void on_write(beast::error_code ec, std::size_t bytes_transferred, const WebSocketMessage& webSocketMessage);
    void maybe_backoff();
    void try_schedule_backoff();
    void schedule_backoff();
    void retry_message(const WebSocketMessage& message);
    void send_signal_update(const std::string& signal_name, const std::string& data);
    void end_session();
    void on_disconnect();
    void resume_sending();
    void remove_sent_message_from_retry_queue();
    bool is_valid_utf8(const std::string& str);
    static std::string truncate_for_log(const std::string& str, size_t max_length = 200)
    {
        if (str.length() <= max_length)
        {
            return str;
        }
        else
        {
            return str.substr(0, max_length) + "...";
        }
    }

    websocket::stream<beast::tcp_stream> ws_;
    WebSocketServer& server_;
    boost::asio::io_context::strand strand_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<RateLimitedLogger> rate_limited_log;
    beast::flat_buffer buffer_;
    std::string session_id_;

    std::deque<WebSocketMessage> outgoing_messages_;
    mutable std::mutex read_mutex_;
    bool writing_ = false;

    std::deque<WebSocketMessage> retry_messages_;
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