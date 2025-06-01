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

        switch (string_to_type_.at(incoming["type"]))
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
                logger_->warn("Unknown message type: {}", incoming["type"].get<std::string>());
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
    if(incoming.contains("signal"))
    {
        if(subscribeToSignal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully Subscribed to " + incoming["signal"].get<std::string>();
            session_.sendMessage(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already subscribed to " + incoming["signal"].get<std::string>();
            session_.sendMessage(response.dump());
        }
    }
    else
    {
        logger_->warn("Subscribe message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Subscribe message missing signal";
        session_.sendMessage(response.dump());
    }
}
void WebSocketSessionMessageManager::handleUnsubscribe(const json& incoming)
{
    logger_->info("Handle unsubscribe message.");
    if(incoming.contains("signal"))
    {
        if(unsubscribeFromSignal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully unsubscribed from " + incoming["signal"].get<std::string>();
            session_.sendMessage(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already unsubscribed from " + incoming["signal"].get<std::string>();
            session_.sendMessage(response.dump());
        }
    }
    else
    {
        logger_->warn("Unsubscribe message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Unsubscribe message missing signal";
        session_.sendMessage(response.dump());
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
    if(incoming.contains("signal"))
    {
        std::string signal_data = incoming["signal"].get<std::string>();
        logger_->warn("Received signal data: {}, not yet handled.");
    }
    else
    {
        logger_->warn("Unsubscribe message without signal.");
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Unsubscribe message missing signal";
        session_.sendMessage(response.dump());
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
    ws_.async_accept(
        net::bind_executor(
            ws_.get_executor(),
            [self = shared_from_this()](beast::error_code ec)
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
}

void WebSocketSession::close()
{
    if (!ws_.is_open())
    {
        logger_->info("WebSocket session already closed.");
        return;
    }

    ws_.async_close(websocket::close_code::normal,
        net::bind_executor(
            ws_.get_executor(),
            [self = shared_from_this()](beast::error_code ec)
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
}

void WebSocketSession::doRead()
{
    ws_.async_read(
        readBuffer_,
        net::bind_executor(
            ws_.get_executor(),
            std::bind(
                &WebSocketSession::onRead,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2)));
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        handleWebSocketError(ec, "onRead");
        return;
    }

    std::string message = beast::buffers_to_string(readBuffer_.data());
    readBuffer_.consume(bytes_transferred);
    logger_->info("Received message: {}", message);
    handleStringMessage(message);
    doRead();
}

void WebSocketSession::doWrite()
{
    if (outgoing_messages_.empty())
        return;

    writing_ = true;
    auto webSocketMessage = std::move(outgoing_messages_.front());
    outgoing_messages_.pop_front();
    switch (webSocketMessage.webSocket_Message_type)
    {
        case WebSocketMessageType::Text:
            if (isValidUtf8(webSocketMessage.message))
            {
                ws_.text(true);
            }
            else 
            {
                logger_->warn("Invalid UTF-8 message, skipping...");
                return;
            }
        break;
        case WebSocketMessageType::Binary:
            ws_.binary(true);
        break;
        default:
            logger_->warn("Unknown WebSocket message type, skipping...");
            return;
        break;
    }
    ws_.async_write(
        net::buffer(webSocketMessage.message),
        net::bind_executor(
            ws_.get_executor(),
            std::bind(
                &WebSocketSession::onWrite,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2,
                webSocketMessage)));
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred, const WebSocketMessage& webSocketMessage)
{
    writing_ = false;
    if (ec)
    {
        handleWebSocketError(ec, "onWrite");
        return;
    }
    logger_->info("Sent {} byte message)", bytes_transferred);
    if (!outgoing_messages_.empty())
    {
        doWrite();
    }
}

void WebSocketSession::sendMessage(const WebSocketMessage& webSocketMessage)
{
    net::post( ws_.get_executor(), [self=shared_from_this(), msg = std::move(webSocketMessage)]() mutable{
        if (self->ws_.is_open())
        {
            if (self->outgoing_messages_.size() <= MAX_QUEUE_SIZE)
            {
                bool write_in_progress = !self->outgoing_messages_.empty();
                self->outgoing_messages_.push_back(std::move(msg));

                if (!write_in_progress)
                {
                    self->doWrite();
                }
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

void WebSocketSession::sendBinaryMessage(const std::vector<uint8_t>& message)
{
    WebSocketMessage binary_msg(message, MessagePriority::Low, false);
    sendMessage(binary_msg);
}

bool WebSocketSession::isValidUtf8(const std::string& str)
{
    try
    {
        boost::locale::conv::from_utf<char>(str, "UTF-8");
        return true;
    }
    catch (const std::exception&)
    {
        logger_->warn("Invalid UTF-8 string: {}", str);
        return false;
    }
}

void WebSocketSession::handleWebSocketError(const std::error_code& ec, const std::string& context)
{
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