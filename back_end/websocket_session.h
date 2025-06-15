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
#include <boost/locale/encoding_utf.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/beast/http.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WebSocketServer;
class MessageTypeHelper
{
public:
    enum class MessageType
    {
        Signal_Subscribe_Message,
        Signal_Unsubscribe_Message,
        Signal_Value_Request_Message,
        Text_Message,
        Signal_Value_Message,
        Echo_Message,
        Unknown
    };

    static MessageType FromString(const std::string& str);
    static std::string ToString(MessageType type);
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
    WebSocketMessage( const std::string& msg, MessagePriority p = MessagePriority::Low, bool retry_flag = false )
                    : message(msg), webSocket_Message_type(WebSocketMessageType::Text), priority(p), should_retry(retry_flag) {}
    WebSocketMessage( const std::vector<uint8_t>& data, MessagePriority p = MessagePriority::Low, bool retry_flag = false )
                    : binary_data(data), webSocket_Message_type(WebSocketMessageType::Binary), priority(p), should_retry(retry_flag) {}
};

class WebSocketSession;

class WebSocketSessionMessageManager : public MessageTypeHelper
{
    public:
        using WebSocketSignalValueCallback = std::function<void(const std::string&, void*)>;
        struct WebSocketSignalValueCallbackData
        {
            WebSocketSignalValueCallback callback;
            void* arg;
        };
        WebSocketSessionMessageManager();
        virtual ~WebSocketSessionMessageManager() = default;
        
        void setSession(std::weak_ptr<WebSocketSession> session);

        bool subscribeToSignal(const std::string& signal_name);
        bool unsubscribeFromSignal(const std::string& signal_name);
        bool isSubscribedToSignal(const std::string& signal_name) const;

        void handleStringMessage(const std::string& message);
        void handleSignalSubscribe(const json& incoming);
        void handleSignalUnsubscribe(const json& incoming);
        void handleSignalValueRequest(const json& incoming);
        void handleTextMessage(const json& incoming);
        void handleSignalValueMessage(const json& incoming);
        void handleEchoMessage(const json& incoming);
        void handleUnknownMessage(const json& incoming);

        std::string createEchoResponse(const std::string& message);
        void sendEchoResponse(const std::string& msg, MessagePriority priority = MessagePriority::Low);

    protected:
        std::weak_ptr<WebSocketSession> session_;
        std::vector<WebSocketSignalValueCallbackData> signal_value_callbacks_;
        mutable std::mutex signal_value_callbacks_mutex_;
        
        std::unordered_set<std::string> subscribed_signals_;
        mutable std::mutex subscription_mutex_;
        
        std::shared_ptr<spdlog::logger> logger_;
};

class WebSocketServer;
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
                       , public WebSocketSessionMessageManager
{
public:
    WebSocketSession(tcp::socket socket, std::shared_ptr<WebSocketServer> server, net::strand<net::io_context::executor_type>& strand);
    WebSocketSession(const WebSocketSession&) = delete;
    WebSocketSession& operator=(const WebSocketSession&) = delete;

    void start();
    void close();
    net::any_io_executor getExecutor()
    {
        return ws_.get_executor();
    }
    void sendMessage(std::shared_ptr<WebSocketMessage> webSocketMessage);
    const std::string& getSessionID() const { return session_id_; }
    bool isRunning() const { return ws_.is_open(); }

    //Signal management
    void subscribeToSignalFromServer(const std::string& signal_name);
    void unsubscribeFromSignalFromServer(const std::string& signal_name);

protected:
    websocket::stream<tcp::socket> ws_;
    std::weak_ptr<WebSocketServer> server_;
    net::strand<net::io_context::executor_type>& strand_;

private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWrite();
    void releaseWritingAndContinue();
    void onWrite(beast::error_code ec, std::size_t bytes_transferred, std::shared_ptr<WebSocketMessage> message);
    bool isValidUtf8(const std::string& str);

    // Handle incoming messages
    void handleTextMessage(std::shared_ptr<WebSocketSession> self, std::shared_ptr<WebSocketMessage> message);
    void handleBinaryMessage(std::shared_ptr<WebSocketSession> self, std::shared_ptr<WebSocketMessage> message);
    void handleUnknownMessageType();
    void handleWebSocketError(const std::error_code& ec, const std::string& context);

    const std::string session_id_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<RateLimitedLogger> rate_limited_log_;

    static constexpr size_t MAX_QUEUE_SIZE = 500;
    std::deque<std::shared_ptr<WebSocketMessage>> outgoing_messages_;
    beast::flat_buffer readBuffer_;
    bool writing_ = false;
    bool closing_ = false;
};