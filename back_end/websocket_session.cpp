#include "websocket_session.h"
#include "websocket_server.h"

#define MAX_BATCH_COUNT 10
#define MAX_RETRY_ATTEMPTS 5
#define BASE_BACKOFF_MS 100

const std::unordered_map<std::string, MessageTypeHelper::MessageType> MessageTypeHelper::string_to_type_ = {
    {"subscribe", MessageTypeHelper::MessageType::Subscribe},
    {"unsubscribe", MessageTypeHelper::MessageType::Unsubscribe},
    {"text", MessageTypeHelper::MessageType::Text},
    {"signal", MessageTypeHelper::MessageType::Signal},
    {"echo", MessageTypeHelper::MessageType::Echo},
    {"unknown", MessageTypeHelper::MessageType::Unknown},
};

const std::unordered_map<MessageTypeHelper::MessageType, std::string> MessageTypeHelper::type_to_string_ = {
    {MessageTypeHelper::MessageType::Subscribe, "subscribe"},
    {MessageTypeHelper::MessageType::Unsubscribe, "unsubscribe"},
    {MessageTypeHelper::MessageType::Text, "text"},
    {MessageTypeHelper::MessageType::Signal, "signal"},
    {MessageTypeHelper::MessageType::Echo, "echo"},
    {MessageTypeHelper::MessageType::Unknown, "unknown"},
};

WebSocketSessionMessageManager::WebSocketSessionMessageManager( WebSocketSession& session )
                                                              : session_(session)
                                                              , logger_(initializeLogger(session_.getSessionID() + " Message Mananger", spdlog::level::info))
{
}

bool WebSocketSessionMessageManager::subscribeToSignal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    if (subscribed_signals_.find(signal_name) == subscribed_signals_.end())
    {
        subscribed_signals_.emplace(signal_name);
        session_.subscribeToSignalFromServer(signal_name);
        logger_->info("Subscribed to signal: {}", signal_name);
        return true;
    }
    else
    {
        logger_->warn("Attempted to subscribe to an already subscribed signal: {}", signal_name);
        return false;
    }
}

bool WebSocketSessionMessageManager::unsubscribeFromSignal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    auto it = subscribed_signals_.find(signal_name);
    if (it != subscribed_signals_.end())
    {
        subscribed_signals_.erase(it);
        session_.unsubscribeFromSignalFromServer(signal_name);
        logger_->info("Unsubscribed from signal: {}", signal_name);
        return true;
    }
    else
    {
        logger_->warn("Attempted to unsubscribe from a signal not subscribed: {}", signal_name);
        return false;
    }
}
                                                              
void WebSocketSessionMessageManager::handleStringMessage(const std::string& message)
{
    json incoming = json::parse(message);
    if (incoming.contains("type"))
    {
        logger_->trace("Message type: {}", incoming["type"].get<std::string>());
        auto type_str = incoming["type"].get<std::string>();
        auto it = string_to_type_.find(type_str);
        if (it != string_to_type_.end()) {
            switch (it->second)
            {
                case MessageType::Subscribe:
                    handleSubscribe(incoming);
                    break;
                case MessageType::Unsubscribe:
                    handleUnsubscribe(incoming);
                    break;
                case MessageType::Text:
                    handleTextMessage(incoming);
                    break;
                case MessageType::Signal:
                    handleSignalMessage(incoming);
                    break;
                default:
                    handleUnknownMessage(incoming);
                    break;
            }
        }
        else 
        {
            logger_->warn("Unknown message type: {}", type_str);
            handleUnknownMessage(incoming);
        }
    }
    else
    {
        logger_->warn("Message does not contain a valid type: {}", message);
    }
}

void WebSocketSessionMessageManager::handleSubscribe(const json& incoming)
{
    logger_->info("Handle subscribe message.");
    auto session = session_.shared_from_this();
    if(incoming.contains("signal"))
    {
        if(subscribeToSignal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully Subscribed to " + incoming["signal"].get<std::string>();
            session->sendMessage(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already subscribed to " + incoming["signal"].get<std::string>();
            session->sendMessage(response.dump());
        }
    }
    else
    {
        logger_->warn("Subscribe message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Subscribe message missing signal";
        session->sendMessage(response.dump());
    }
}

void WebSocketSessionMessageManager::handleUnsubscribe(const json& incoming)
{
    logger_->info("Handle unsubscribe message.");
    auto session = session_.shared_from_this();
    if(incoming.contains("signal"))
    {
        if(unsubscribeFromSignal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully unsubscribed from " + incoming["signal"].get<std::string>();
            session->sendMessage(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already unsubscribed from " + incoming["signal"].get<std::string>();
            session->sendMessage(response.dump());
        }
    }
    else
    {
        logger_->warn("Unsubscribe message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Unsubscribe message missing signal";
        session->sendMessage(response.dump());
    }
}

bool WebSocketSessionMessageManager::isSubscribedToSignal(const std::string& signal_name) const
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    return subscribed_signals_.find(signal_name) != subscribed_signals_.end();
}

void WebSocketSessionMessageManager::handleTextMessage(const json& incoming)
{
    logger_->warn("Received text message: {} Not Yet Handled.", incoming.dump());
}

void WebSocketSessionMessageManager::handleSignalMessage(const json& incoming)
{
    auto session = session_.shared_from_this();
    if(incoming.contains("signal"))
    {
        std::string signal_data = incoming["signal"].get<std::string>();
        logger_->warn("Received signal data: {}, not yet handled.");
    }
    else
    {
        logger_->warn("Signal message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Signal message missing signal";
        session->sendMessage(response.dump());
    }
}

void WebSocketSessionMessageManager::handleEchoMessage(const json& incoming)
{
    logger_->warn("Received echo message: {} Not Yet Handled", incoming.dump());
}

void WebSocketSessionMessageManager::handleUnknownMessage(const json& incoming)
{
    logger_->warn("Received unknown message: {}", incoming.dump()); 
}

WebSocketSession::WebSocketSession(tcp::socket socket, std::shared_ptr<WebSocketServer> server)
    : ws_(std::move(socket))
    , server_(server)
    , session_id_(boost::uuids::to_string(boost::uuids::random_generator()()))
    , logger_(initializeLogger("WebSocketSession", spdlog::level::info))
    , rate_limited_log_(std::make_shared<RateLimitedLogger>(logger_, std::chrono::seconds(10)))
    , WebSocketSessionMessageManager(*this)
{
}

void WebSocketSession::start()
{
    auto self = shared_from_this();
    net::post(ws_.get_executor(), [self]()
    {
        self->ws_.async_accept(
        net::bind_executor(
            self->ws_.get_executor(),
            [self](beast::error_code ec)
            {
                if (ec)
                {
                    self->handleWebSocketError(ec, "start");
                    return;
                }
                else
                {
                    self->logger_->info("WebSocket session started successfully.");
                }
                self->doRead();
            }));
    });
}

void WebSocketSession::close()
{
    auto self = shared_from_this();
    net::post(ws_.get_executor(), [self]()
    {
        if (!self->ws_.is_open())
        {
            self->logger_->info("WebSocket session already closed.");
            return;
        }
        if (self->closing_)
        {
            self->logger_->info("WebSocket session is already closing.");
            return;
        }

        self->closing_ = true;

        self->ws_.async_close(websocket::close_code::normal,
            net::bind_executor(self->ws_.get_executor(),
                [self](beast::error_code ec)
                {
                    if (ec)
                    {
                        self->handleWebSocketError(ec, "close");
                    }
                    else
                    {
                        self->logger_->info("WebSocket session closed cleanly.");
                    }

                    if (auto server = self->server_.lock())
                        server->unregisterSession(self);
        }));
    });
}

void WebSocketSession::doRead()
{
    auto self = shared_from_this();
    net::post(ws_.get_executor(), [self]()
    {
        if (!self->ws_.is_open())
        {
            self->logger_->warn("WebSocket session is not open, cannot read.");
            return;
        }
        if (self->closing_)
        {
            self->logger_->warn("WebSocket session is closing, cannot read.");
            return;
        }
        self->ws_.async_read(
            self->readBuffer_,
            net::bind_executor(
                self->ws_.get_executor(),
                [self](beast::error_code ec, std::size_t bytes_transferred)
                {
                    self->onRead(ec, bytes_transferred);
                }));
    });
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        if (ec == boost::beast::websocket::error::closed)
        {
            logger_->info("WebSocket closed by client.");
            close();
            return;
        }
        else if (ec == net::error::operation_aborted)
        {
            logger_->info("Operation aborted, likely due to shutdown.");
            close();
            return;
        }
        handleWebSocketError(ec, "onRead");
        return;
    }
    std::string message = beast::buffers_to_string(readBuffer_.data());
    readBuffer_.consume(bytes_transferred);
    logger_->info("Received message: {}", message);
    handleStringMessage(message);
    doRead();
}

void WebSocketSession::sendMessage(const WebSocketMessage& webSocketMessage)
{
    auto self = shared_from_this();
    net::post(ws_.get_executor(), [self, msg = std::move(webSocketMessage)]() mutable
    {
        if (self->ws_.is_open())
        {
            if (self->outgoing_messages_.size() <= MAX_QUEUE_SIZE)
            {
                self->outgoing_messages_.push_back(std::move(msg));
                self->doWrite();
            }
            else
            {
                self->logger_->warn("Dropping message: queue full");
            }
        }
        else
        {
            self->logger_->warn("Attempted to send on closed WebSocket");
        }
    });
}

void WebSocketSession::doWrite()
{
    auto self = shared_from_this();
    net::post(ws_.get_executor(), [self]()
    {
        std::shared_ptr<WebSocketMessage> webSocketMessage;
        if (!self->ws_.is_open())
        {
            self->logger_->warn("WebSocket session is not open, cannot read.");
            return;
        }
        if (self->closing_)
        {
            self->logger_->warn("WebSocket session is closing, cannot read.");
            return;
        }
        if (self->writing_ || self->outgoing_messages_.empty())
        {
            return;
        }
        
        self->writing_ = true;
        webSocketMessage = std::make_shared<WebSocketMessage>(std::move(self->outgoing_messages_.front()));
        self->outgoing_messages_.pop_front();
        switch (webSocketMessage->webSocket_Message_type)
        {
            case WebSocketMessageType::Text:
                self->handleTextMessage(self, webSocketMessage);
                break;

            case WebSocketMessageType::Binary:
                self->handleBinaryMessage(self, webSocketMessage);
                break;

            default:
                self->handleUnknownMessageType();
                break;
        }
    });
}

void WebSocketSession::releaseWritingAndContinue()
{
    writing_ = false;
    auto self = shared_from_this();
    self->doWrite();
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred, const WebSocketMessage& webSocketMessage)
{
    if (ec)
    {
        handleWebSocketError(ec, "onWrite");
    }
    else
    {
        if(webSocketMessage.webSocket_Message_type == WebSocketMessageType::Text)
        {
            std::string message = webSocketMessage.message;
            message.resize(200);
            logger_->debug("Sent text message: {}", message);
        }
        else if(webSocketMessage.webSocket_Message_type == WebSocketMessageType::Binary)
        {
            logger_->debug("Sent binary message of size: {} bytes", webSocketMessage.binary_data.size());
        }
    }
    writing_ = false;
    doWrite();
}

void WebSocketSession::sendBinaryMessage(const std::vector<uint8_t>& message)
{
    WebSocketMessage binary_msg(message, MessagePriority::Low, false);
    sendMessage(binary_msg);
}

bool WebSocketSession::isValidUtf8(const std::string& str)
{
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str.data());
    size_t length = str.size();
    size_t i = 0;

    while (i < length)
    {
        if (bytes[i] <= 0x7F)
        {
            // ASCII byte
            i++;
        }
        else if ((bytes[i] & 0xE0) == 0xC0)
        {
            // 2-byte sequence
            if (i + 1 >= length) return false;
            if ((bytes[i+1] & 0xC0) != 0x80) return false;
            if (bytes[i] < 0xC2) return false; // overlong encoding check
            i += 2;
        }
        else if ((bytes[i] & 0xF0) == 0xE0)
        {
            // 3-byte sequence
            if (i + 2 >= length) return false;
            if ((bytes[i+1] & 0xC0) != 0x80) return false;
            if ((bytes[i+2] & 0xC0) != 0x80) return false;

            // overlong check for 3-byte sequence
            if (bytes[i] == 0xE0 && bytes[i+1] < 0xA0) return false;

            // UTF-16 surrogate halves are invalid in UTF-8
            if (bytes[i] == 0xED && bytes[i+1] >= 0xA0) return false;

            i += 3;
        }
        else if ((bytes[i] & 0xF8) == 0xF0)
        {
            // 4-byte sequence
            if (i + 3 >= length) return false;
            if ((bytes[i+1] & 0xC0) != 0x80) return false;
            if ((bytes[i+2] & 0xC0) != 0x80) return false;
            if ((bytes[i+3] & 0xC0) != 0x80) return false;

            // overlong check for 4-byte sequence
            if (bytes[i] == 0xF0 && bytes[i+1] < 0x90) return false;

            // restrict to valid Unicode range (<= U+10FFFF)
            if (bytes[i] > 0xF4) return false;

            i += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

void WebSocketSession::handleTextMessage(
    std::shared_ptr<WebSocketSession> self,
    std::shared_ptr<WebSocketMessage> message)
{
    if (!isValidUtf8(message->message))
    {
        logger_->warn("Invalid UTF-8 message, skipping...");
        releaseWritingAndContinue();
        return;
    }
    ws_.text(true);
    ws_.async_write(
        net::buffer(message->message),
        net::bind_executor(
            ws_.get_executor(),
            [self, message](boost::system::error_code ec, std::size_t bytes_transferred)
            {
                self->onWrite(ec, bytes_transferred, *message);
            }));
}


void WebSocketSession::handleBinaryMessage(
    std::shared_ptr<WebSocketSession> self,
    std::shared_ptr<WebSocketMessage> message)
{
    if (message->binary_data.empty())
    {
        logger_->warn("Empty binary message, skipping...");
        releaseWritingAndContinue();
        return;
    }
    ws_.text(false);
    ws_.async_write(
        net::buffer(message->binary_data),
        net::bind_executor(
            ws_.get_executor(),
            [self, message](boost::system::error_code ec, std::size_t bytes_transferred)
            {
                self->onWrite(ec, bytes_transferred, *message);
            }));
}

void WebSocketSession::handleUnknownMessageType()
{
    logger_->warn("Unknown WebSocket message type, skipping...");
    releaseWritingAndContinue();
}

void WebSocketSession::handleWebSocketError(const std::error_code& ec, const std::string& context)
{
    if (closing_)
    {
        logger_->debug("WebSocket session is already closing, ignoring error: {}", ec.message());
        return;
    }
    if (!ec) return;

    const std::string sid = getSessionID();
    const std::string ctx = context.empty() ? "" : " (" + context + ")";
    const int code = ec.value();
    switch (code)
    {
        case ECONNREFUSED:
            logger_->error("Connection refused{} (code {}): server may be down or rejecting connections.", ctx, code);
            close();
            break;
        case ECONNRESET:
            logger_->error("Connection reset{} (code {}): peer closed the connection unexpectedly.", ctx, code);
            close();
            break;
        case ECONNABORTED:
            logger_->error("Connection aborted{} (code {})", ctx, code);
            close();
            break;
        case ETIMEDOUT:
        case EHOSTUNREACH:
        case ENETUNREACH:
        case EALREADY:
        case EINPROGRESS:
        case ENOBUFS:
        case EMFILE:
        case ENFILE:
            logger_->error("Recoverable network issue{} (code {}) for session {}: {}", ctx, code, sid, ec.message());
            //try_schedule_backoff();
            break;
        case ENOTCONN:
        case EBADF:
        case ENOTSOCK:
        case EPIPE:
        case EPROTO:
        case EMSGSIZE:
        case EPERM:
        case EACCES:
        case EINVAL:
            logger_->error("Fatal error{} (code {}) for session {}: {}", ctx, code, sid, ec.message());
            close();
            break;
        case EISCONN:
        case EAGAIN:
            logger_->error("Socket state warning{} (code {}): {}", ctx, code, ec.message());
            break;
        case EADDRINUSE:
            logger_->error("Address already in use{} (code {}): {}", ctx, code, ec.message());
            close();
            break;
        default:
            logger_->error("Unhandled WebSocket error{} (code {}): {}", ctx, code, ec.message());
            close();
            break;
    }
}

void WebSocketSession::subscribeToSignalFromServer(const std::string& signal_name)
{
    if (auto s = server_.lock())
    {
        s->subscribe_session_to_signal(getSessionID(), signal_name);
    }
}

void WebSocketSession::unsubscribeFromSignalFromServer(const std::string& signal_name)
{
    if (auto s = server_.lock())
    {
        s->unsubscribe_session_from_signal(getSessionID(), signal_name);
    }
}