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

WebSocketSessionMessageManager::WebSocketSessionMessageManager()
{
}

void WebSocketSessionMessageManager::setSession(std::weak_ptr<WebSocketSession> session)
{
    session_ = std::move(session);
    auto locked_session = session_.lock();

    if (locked_session)
    {
        if (!logger_)
        {
            logger_ = initializeLogger(locked_session->getSessionID() + " Message Manager", spdlog::level::info);
        }
    }
    else
    {
        if (!logger_)
        {
            logger_ = initializeLogger("Unknown Session Message Manager", spdlog::level::info);
        }
    }
}

bool WebSocketSessionMessageManager::subscribeToSignal(const std::string& signal_name)
{
    auto session = session_.lock();
    if (!session)
    {
        logger_->warn("sendEchoResponse failed: session expired");
        return false;
    }
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    auto result = subscribed_signals_.emplace(signal_name);
    if (result.second)
    {
        session->subscribeToSignalFromServer(signal_name);
        logger_->info("Subscribed to signal: {}", signal_name);
        return true;
    }
    logger_->warn("Attempted to subscribe to an already subscribed signal: {}", signal_name);
    return false;
}

bool WebSocketSessionMessageManager::unsubscribeFromSignal(const std::string& signal_name)
{
    auto session = session_.lock();
    if (!session)
    {
        logger_->warn("sendEchoResponse failed: session expired");
        return false;
    }
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    auto erased = subscribed_signals_.erase(signal_name);
    if (erased > 0)
    {
        session->unsubscribeFromSignalFromServer(signal_name);
        logger_->info("Unsubscribed from signal: {}", signal_name);
        return true;
    }
    logger_->warn("Attempted to unsubscribe from a signal not subscribed: {}", signal_name);
    return false;
}
                                                              
void WebSocketSessionMessageManager::handleStringMessage(const std::string& message)
{
    json incoming;
    try
    {
        logger_->trace("Message: {}", message);
        incoming = json::parse(message);
    }
    catch (const json::parse_error& e)
    {
        logger_->warn("Failed to parse JSON message: {} Error: {}", message, e.what());
        return;
    }

    if (!incoming.contains("type"))
    {
        logger_->warn("Message does not contain a valid type: {}", message);
        return;
    }

    auto type_str = incoming.value("type", "");
    logger_->trace("Message type: {}", type_str);

    auto it = string_to_type_.find(type_str);
    if (it == string_to_type_.end())
    {
        logger_->warn("Unknown message type: {}", type_str);
        handleUnknownMessage(incoming);
        return;
    }

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

void WebSocketSessionMessageManager::handleSubscribe(const json& incoming)
{
    logger_->info("Handle subscribe message.");
    if(incoming.contains("signal") && incoming["signal"].is_string())
    {
        if(subscribeToSignal(incoming["signal"]))
        {
            sendEchoResponse("Successfully Subscribed to " + incoming["signal"].get<std::string>());
        }
        else
        {
            sendEchoResponse("Already subscribed to " + incoming["signal"].get<std::string>());
        }
    }
    else
    {
        logger_->warn("Subscribe message without signal.");
        sendEchoResponse("Subscribe message missing signal");
    }
}

void WebSocketSessionMessageManager::handleUnsubscribe(const json& incoming)
{
    logger_->info("Handle unsubscribe message.");
    if(incoming.contains("signal") && incoming["signal"].is_string())
    {
        if(unsubscribeFromSignal(incoming["signal"]))
        {
            sendEchoResponse("Successfully unsubscribed from " + incoming["signal"].get<std::string>());
        }
        else
        {
            sendEchoResponse("Already unsubscribed from " + incoming["signal"].get<std::string>());
        }
    }
    else
    {
        logger_->warn("Unsubscribe message without signal.");
        sendEchoResponse("Unsubscribe message missing signal");
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
    if(incoming.contains("signal") && incoming["signal"].is_string())
    {
        std::string signal_data = incoming["signal"].get<std::string>();
        logger_->warn("Received signal data: {}, not yet handled.", signal_data);
    }
    else
    {
        logger_->warn("Signal message without signal.");
        sendEchoResponse("Signal message missing signal");
    }
}

std::string WebSocketSessionMessageManager::createEchoResponse(const std::string& message)
{
    json response;
    response["type"] = MessageTypeHelper::type_to_string_.at(MessageTypeHelper::MessageType::Echo);
    response["message"] = message;
    return response.dump();
}

void WebSocketSessionMessageManager::sendEchoResponse(const std::string& msg, MessagePriority priority)
{
    auto session = session_.lock();
    if (!session)
    {
        logger_->warn("sendEchoResponse failed: session expired");
        return;
    }
    auto echo = std::make_shared<WebSocketMessage>(createEchoResponse(msg), priority, false);
    session->sendMessage(std::move(echo));
}

void WebSocketSessionMessageManager::handleEchoMessage(const json& incoming)
{
    logger_->warn("Received echo message: {} Not Yet Handled", incoming.dump());
}

void WebSocketSessionMessageManager::handleUnknownMessage(const json& incoming)
{
    logger_->warn("Received unknown message: {}", incoming.dump()); 
}

WebSocketSession::WebSocketSession(tcp::socket socket, std::shared_ptr<WebSocketServer> server, net::strand<net::io_context::executor_type>& strand)
    : ws_(std::move(socket))
    , server_(server)
    , strand_(strand)
    , session_id_(boost::uuids::to_string(boost::uuids::random_generator()()))
    , logger_(initializeLogger("WebSocketSession", spdlog::level::info))
    , rate_limited_log_(std::make_shared<RateLimitedLogger>(logger_, std::chrono::seconds(10)))
    , WebSocketSessionMessageManager()
{
}

void WebSocketSession::start()
{
    auto self = shared_from_this();
    WebSocketSessionMessageManager::setSession(self);
    net::post(strand_, [self]()
    {
        self->ws_.async_accept(
            net::bind_executor(self->strand_,
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
    net::post(strand_, [self]()
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
            net::bind_executor(self->strand_,
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
    net::post(strand_, [self]()
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
        net::bind_executor(self->strand_,
        [self](beast::error_code ec, std::size_t bytes_transferred)
        {
            self->onRead(ec, bytes_transferred);
        }));
    });
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    assert(strand_.running_in_this_thread());
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

void WebSocketSession::sendMessage(std::shared_ptr<WebSocketMessage> webSocketMessage)
{
    auto self = shared_from_this();
    net::post(strand_, [self, wsm = std::move(webSocketMessage)]()
    {
        if (self->ws_.is_open())
        {
            if (self->outgoing_messages_.size() <= MAX_QUEUE_SIZE)
            {
                self->outgoing_messages_.push_back(std::move(wsm));
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
    net::post(strand_, [self]()
    {
        if (!self->ws_.is_open())
        {
            self->logger_->warn("WebSocket session is not open, cannot write.");
            return;
        }

        if (self->closing_)
        {
            self->logger_->warn("WebSocket session is closing, cannot write.");
            return;
        }

        if (self->writing_ || self->outgoing_messages_.empty())
        {
            return;
        }

        self->writing_ = true;

        std::shared_ptr<WebSocketMessage> webSocketMessage = std::move(self->outgoing_messages_.front());
        self->outgoing_messages_.pop_front();

        switch (webSocketMessage->webSocket_Message_type)
        {
            case WebSocketMessageType::Text:
                self->handleTextMessage(self, std::move(webSocketMessage));
                break;

            case WebSocketMessageType::Binary:
                self->handleBinaryMessage(self, std::move(webSocketMessage));
                break;

            default:
                self->handleUnknownMessageType();
                break;
        }
        
    });
}


void WebSocketSession::releaseWritingAndContinue()
{
    assert(strand_.running_in_this_thread());
    writing_ = false;
    doWrite();
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred, std::shared_ptr<WebSocketMessage> webSocketMessage)
{
    assert(strand_.running_in_this_thread());
    if (ec)
    {
        handleWebSocketError(ec, "onWrite");
    }
    else
    {
        if(webSocketMessage->webSocket_Message_type == WebSocketMessageType::Text)
        {
            std::string message = webSocketMessage->message;
            message.resize(200);
            logger_->debug("Sent text message: {}", message);
        }
        else if(webSocketMessage->webSocket_Message_type == WebSocketMessageType::Binary)
        {
            logger_->debug("Sent binary message of size: {} bytes", webSocketMessage->binary_data.size());
        }
    }
    writing_ = false;
    doWrite();
}

bool WebSocketSession::isValidUtf8(const std::string& str)
{
    try
    {
        // Attempt to convert UTF-8 string to UTF-32. Will throw on invalid input.
        boost::locale::conv::utf_to_utf<char32_t>(str);
        return true;
    }
    catch (const boost::locale::conv::conversion_error&)
    {
        return false;
    }
}

void WebSocketSession::handleTextMessage( std::shared_ptr<WebSocketSession> self
                                        , std::shared_ptr<WebSocketMessage> webSocketMessage )
{
    assert(strand_.running_in_this_thread());
    if (!isValidUtf8(webSocketMessage->message))
    {
        logger_->warn("Invalid UTF-8 message, skipping...");
        releaseWritingAndContinue();
        return;
    }
    if (!ws_.is_open())
    {
        logger_->warn("WebSocket is not open, aborting text write");
        releaseWritingAndContinue();
        return;
    }
    ws_.text(true);
    ws_.async_write(
        net::buffer(webSocketMessage->message),
        net::bind_executor(self->strand_,
        [self, wsm = std::move(webSocketMessage)](boost::system::error_code ec, std::size_t bytes_transferred)
        {
            self->onWrite(ec, bytes_transferred, std::move(wsm));
        }));
}


void WebSocketSession::handleBinaryMessage( std::shared_ptr<WebSocketSession> self
                                          , std::shared_ptr<WebSocketMessage> webSocketMessage )
{
    assert(strand_.running_in_this_thread());
    if (webSocketMessage->binary_data.empty())
    {
        logger_->warn("Empty binary message, skipping...");
        releaseWritingAndContinue();
        return;
    }
    if (!ws_.is_open())
    {
        logger_->warn("WebSocket is not open, aborting binary write");
        releaseWritingAndContinue();
        return;
    }
    ws_.binary(true);
    ws_.async_write(
        net::buffer(webSocketMessage->binary_data.data(), webSocketMessage->binary_data.size()),
        net::bind_executor(self->strand_,
        [self, wsm = std::move(webSocketMessage)](boost::system::error_code ec, std::size_t bytes_transferred)
        {
            self->onWrite(ec, bytes_transferred, std::move(wsm));
        }));
}

void WebSocketSession::handleUnknownMessageType()
{
    assert(strand_.running_in_this_thread());
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
    assert(strand_.running_in_this_thread());
    if (auto s = server_.lock())
    {
        s->subscribe_session_to_signal(getSessionID(), signal_name);
    }
}

void WebSocketSession::unsubscribeFromSignalFromServer(const std::string& signal_name)
{
    assert(strand_.running_in_this_thread());
    if (auto s = server_.lock())
    {
        s->unsubscribe_session_from_signal(getSessionID(), signal_name);
    }
}