#include "websocket_session.h"
#include "websocket_server.h"

#define MAX_BATCH_COUNT 10
#define MAX_RETRY_ATTEMPTS 5
#define BASE_BACKOFF_MS 100 // Example backoff delay base

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

WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket))
    , server_(server)
    , strand_(server_.get_io_context())
    , backoff_timer_(ws_.get_executor())
{
    logger_ = InitializeLogger("Web Socket Session", spdlog::level::info);
    rate_limited_log = std::make_shared<RateLimitedLogger>(logger_, std::chrono::milliseconds(10000));
    session_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
    logger_->info("Created new WebSocket session: {}", session_id_);
}

void WebSocketSession::run()
{
    logger_->debug("Session: {} Run.", GetSessionID());
    ws_.async_accept(
        [self = shared_from_this()](beast::error_code ec)
        {
            if (!ec)
            {
                self->do_read();
            }
            else
            {
                self->logger_->warn("Accept error: {}", ec.message());
                self->end_session();
            }
        });
}

void WebSocketSession::close()
{
    if (!ws_.is_open())
    {
        logger_->info("Session {} already closed.", GetSessionID());
        return;
    }
    logger_->debug("Session: {} Async Close.", GetSessionID());
    ws_.async_close(websocket::close_code::normal,
        [self = shared_from_this()](beast::error_code ec)
        {
            if (ec)
            {
                self->logger_->warn("Error during async_close: {}", ec.message());
            }
            else
            {
                self->logger_->info("Session {} closed cleanly.", self->GetSessionID());
                self->end_session();
            }
        });
}

std::string WebSocketSession::GetSessionID() const
{
    return session_id_;
}

void WebSocketSession::send_message(const WebSocketMessage& webSocketMessage)
{
    auto self = shared_from_this();
    asio::post(strand_, [self, webSocketMessage]() {
        if (self->outgoing_messages_.size() >= MAX_QUEUE_SIZE)
        {
            self->try_schedule_backoff();
            return;
        }

        bool write_in_progress = !self->outgoing_messages_.empty();
        self->outgoing_messages_.push_back(std::move(webSocketMessage));

        if (!write_in_progress && !self->backoff_enabled_)
        {
            self->do_write();
        }
    });
}

void WebSocketSession::send_binary_message(const std::vector<uint8_t>& message)
{
    WebSocketMessage binary_msg(message, MessagePriority::Low, false);
    send_message(binary_msg);
}

void WebSocketSession::try_schedule_backoff()
{
    if (!backoff_enabled_)
    {
        backoff_enabled_ = true;
        backoff_attempts_++;
        schedule_backoff();
    }
}

void WebSocketSession::schedule_backoff()
{
    logger_->warn("Backoff enabled for session {}, queue size {}", session_id_, outgoing_messages_.size());
    int delay = BASE_BACKOFF_MS * (1 << std::min(backoff_attempts_, 6));
    backoff_timer_.expires_after(std::chrono::milliseconds(delay));
    backoff_timer_.async_wait([self = shared_from_this()](const beast::error_code& ec)
    {
        if (!ec)
        {
            self->resume_sending();
        }
    });
}

void WebSocketSession::resume_sending()
{
    auto self = shared_from_this();
    asio::post(strand_, [self]()
    {        
        self->backoff_enabled_ = false;
        while (!self->retry_messages_.empty())
        {
            auto& msg = self->retry_messages_.front();
            if (msg.retry_count < MAX_RETRY_ATTEMPTS)
            {
                ++msg.retry_count;
                self->outgoing_messages_.push_front(std::move(msg));
            }
            else
            {
                self->logger_->warn("Dropping message after {} retries: {}", MAX_RETRY_ATTEMPTS, msg.message);
            }
            self->retry_messages_.pop_front();
        }
        if (!self->outgoing_messages_.empty() && !self->backoff_enabled_)
        {
            self->do_write();
        }
    });
}

bool WebSocketSession::is_valid_utf8(const std::string& str)
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


void WebSocketSession::do_read()
{
    logger_->debug("Session: {} Do Read.", GetSessionID());
    std::lock_guard<std::mutex> lock(read_mutex_);
    ws_.async_read(buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                self->handleWebSocketError(ec);
                return;
            }
            self->on_read(bytes_transferred);
            self->do_read();
        });
}

void WebSocketSession::handleWebSocketError(const std::error_code& ec, const std::string& context)
{
    if (!ec)
    {
        return;
    }

    auto sid = GetSessionID();

    switch (ec.value())
    {
        case ECONNREFUSED:
            logger_->error("{}: Connection refused: server may be down or rejecting connections.", sid);
            try_schedule_backoff();
            break;
        case ECONNRESET:
            logger_->error("{}: Connection reset: peer closed the connection unexpectedly.", sid);
            try_schedule_backoff();
            break;
        case ECONNABORTED:
            logger_->error("{}: Connection aborted.", sid);
            try_schedule_backoff();
            break;
        case ETIMEDOUT:
            logger_->error("{}: Operation timed out.", sid);
            try_schedule_backoff();
            break;
        case EHOSTUNREACH:
        case ENETUNREACH:
            logger_->error("{}: Network unreachable.", sid);
            try_schedule_backoff();
            break;
        case ENOTCONN:
            logger_->error("{}: Socket not connected.", sid);
            end_session();
            break;
        case EISCONN:
            logger_->error("{}: Socket already connected.", sid);
            end_session();
            break;
        case EADDRINUSE:
            logger_->error("{}: Address in use: another process may be using this port.", sid);
            try_schedule_backoff();
            break;
        case EALREADY:
        case EINPROGRESS:
            logger_->error("{}: Connection already in progress.", sid);
            try_schedule_backoff();
            break;
        case EAGAIN:
            logger_->error("{}: Operation would block (non-blocking mode).", sid);
            break;
        case ENOBUFS:
            logger_->error("{}: No buffer space available.", sid);
            try_schedule_backoff();
            break;
        case EMFILE:
        case ENFILE:
            logger_->error("{}: Too many open files: descriptor limit reached.", sid);
            try_schedule_backoff();
            break;
        case EBADF:
        case ENOTSOCK:
            logger_->error("{}: Invalid socket or file descriptor.", sid);
            end_session();
            break;
        case EPIPE:
            logger_->error("{}: Broken pipe: attempted to write to a closed connection.", sid);
            end_session();
            break;
        case EPROTO:
            logger_->error("{}: Protocol error: invalid WebSocket message or handshake.", sid);
            end_session();
            break;
        case EMSGSIZE:
            logger_->error("{}: Message too large.", sid);
            end_session();
            break;
        case EPERM:
        case EACCES:
        case EINVAL:
            logger_->error("{}: Permission denied or invalid argument.", sid);
            end_session();
            break;
        default:
            logger_->error("{}: Unhandled WebSocket error: {}", sid, ec.message());
            end_session();
            break;
    }

    std::cerr << "[WebSocket Error] "
              << (context.empty() ? "" : context + ": ")
              << ec.message() << " (code: " << ec.value() << ", sid: " << sid << ")" << std::endl;
}

bool WebSocketSession::is_subscribed_to(const std::string& signal_name) const
{
    return subscribed_signals_.find(signal_name) != subscribed_signals_.end();
}

void WebSocketSession::on_read(std::size_t)
{
    logger_->debug("Session: {} On Read.", GetSessionID());
    
    std::lock_guard<std::mutex> lock(read_mutex_);
    auto message = beast::buffers_to_string(buffer_.data());
    logger_->info("Received: {}", message);
    buffer_.consume(buffer_.size());
    try
    {
        json incoming = json::parse(message);

        if (incoming.contains("type"))
        {
            logger_->trace("Message type: {}", incoming["type"].get<std::string>());

            switch (string_to_type_.at(incoming["type"]))
            {
                case MessageType::Subscribe:
                    handle_subscribe(incoming);
                    break;
                case MessageType::Unsubscribe:
                    handle_unsubscribe(incoming);
                    break;
                case MessageType::Text:
                    handle_text_message(incoming);
                    break;
                case MessageType::Signal:
                    handle_signal_message(incoming);
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
    catch (const std::exception& e)
    {
        logger_->error("Exception while parsing message: {}", e.what());
    }
}

void WebSocketSession::handle_subscribe(const json& incoming)
{
    if(incoming.contains("signal"))
    {
        if(subscribe_to_signal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully Subscribed to " + incoming["signal"].get<std::string>();
            send_message(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already subscribed to " + incoming["signal"].get<std::string>();
            send_message(response.dump());
        }
    }
    else
    {
        logger_->warn("Session: {} Subscribe Message without signal", GetSessionID());
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Subscribe message missing signal";
        send_message(response.dump());
    }
}
void WebSocketSession::handle_unsubscribe(const json& incoming)
{
    logger_->info("Session: {} Unsubscribe Message", GetSessionID());
    if(incoming.contains("signal"))
    {
        if(unsubscribe_from_signal(incoming["signal"]))
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Successfully unsubscribed from " + incoming["signal"].get<std::string>();
            send_message(response.dump());
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Already unsubscribed from " + incoming["signal"].get<std::string>();
            send_message(response.dump());
        }
    }
    else
    {
        logger_->warn("Session: {} Unsubscribe Message without signal", GetSessionID());
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Unsubscribe message missing signal";
        send_message(response.dump());
    }
    
}

void WebSocketSession::handle_text_message(const json& incoming)
{
    logger_->warn("Received text message: {} Not Yet Handled", incoming.dump());
}

void WebSocketSession::handle_signal_message(const json& incoming)
{
    if(incoming.contains("signal"))
    {
        std::string signal_data = incoming["signal"].get<std::string>();
        logger_->warn("Received signal data: {} Not Yet Handled", signal_data);
        // Handle signal data processing
    }
    else
    {
        logger_->warn("Session: {} Unsubscribe Message without signal", GetSessionID());
        json response;
        response["type"] = type_to_string_.at(MessageType::Echo);
        response["message"] = "Unsubscribe message missing signal";
        send_message(response.dump());
    }
}

void WebSocketSession::handle_echo_message(const json& incoming)
{
    
}

void WebSocketSession::handle_unknown_message(const json& incoming)
{
    logger_->warn("Received unknown message: {}", incoming.dump()); 
}

void WebSocketSession::do_write()
{
    if (writing_ || outgoing_messages_.empty() || backoff_enabled_)
    {
        return;
    }

    WebSocketMessage webSocketMessage = std::move(outgoing_messages_.front());
    outgoing_messages_.pop_front();
    writing_ = true;

    auto self = shared_from_this();

    switch (webSocketMessage.webSocket_Message_type)
    {
        case WebSocketMessageType::Text:
        {
            if (!is_valid_utf8(webSocketMessage.message))
            {
                logger_->warn("Session {}: Invalid UTF-8 message, skipping...", session_id_);
                writing_ = false;
                asio::post(strand_, [self]() { self->do_write(); });
                return;
            }

            ws_.text(true);
            ws_.async_write(
                asio::buffer(webSocketMessage.message),
                asio::bind_executor(
                    strand_,
                    [self, msg = std::move(webSocketMessage)](beast::error_code ec, std::size_t bytes_transferred)
                    {
                        self->on_write(ec, bytes_transferred, msg);
                    }));
            break;
        }

        case WebSocketMessageType::Binary:
        {
            ws_.binary(true);
            ws_.async_write(
                asio::buffer(webSocketMessage.binary_data),
                asio::bind_executor(
                    strand_,
                    [self, msg = std::move(webSocketMessage)](beast::error_code ec, std::size_t bytes_transferred)
                    {
                        self->on_write(ec, bytes_transferred, msg);
                    }));
            break;
        }

        default:
            logger_->warn("Session {}: Unknown message type", session_id_);
            writing_ = false;
            asio::post(strand_, [self]() { self->do_write(); });  // try next message
            break;
    }
}


void WebSocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred, const WebSocketMessage& webSocketMessage)
{
    auto self = shared_from_this();
    asio::post(strand_, [self, ec, bytes_transferred, webSocketMessage]() mutable {
        self->writing_ = false;
        if (ec)
        {
            self->rate_limited_log->log("write error", spdlog::level::warn, "Write error: {}", ec.message());
            self->try_schedule_backoff();
            if (webSocketMessage.should_retry)
            {
                self->retry_message(webSocketMessage);
            }
        }
        else
        {
            self->rate_limited_log->log("write success", spdlog::level::debug,
                                        "Sent message: {}, bytes: {}", webSocketMessage.message, bytes_transferred);
        }

        if (!self->outgoing_messages_.empty() && !self->backoff_enabled_)
        {
            self->do_write();
        }
    });
}

void WebSocketSession::retry_message(const WebSocketMessage& webSocketMessage)
{
    std::lock_guard<std::mutex> lock(retry_mutex_);
    if (retry_messages_.size() < MAX_BATCH_COUNT)
    {
        retry_messages_.push_back(webSocketMessage);
        rate_limited_log->log("Queued for retry", spdlog::level::info, "Queued message for retry: {}", truncate_for_log(webSocketMessage.message));
    }
    else
    {
        rate_limited_log->log("retry queue full", spdlog::level::warn, "Retry queue full, dropping message: {}", truncate_for_log(webSocketMessage.message));
    }
}

bool WebSocketSession::subscribe_to_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    if (subscribed_signals_.find(signal_name) == subscribed_signals_.end())
    {
        subscribed_signals_.emplace(signal_name);
        logger_->info("Session: {} Subscribed to signal: {}", GetSessionID(), signal_name);
        return true;
    }
    return false;
}

bool WebSocketSession::unsubscribe_from_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    auto it = subscribed_signals_.find(signal_name);
    if (it != subscribed_signals_.end())
    {
        subscribed_signals_.erase(it);
        logger_->info("Session: {} Unsubscribed from signal: {}", GetSessionID(), signal_name);
        return true;
    }
    return false;
}

void WebSocketSession::send_signal_update(const std::string& signal_name, const std::string& data)
{
    json update_msg;
    update_msg["type"] = "signal";
    update_msg["signal"] = signal_name;
    update_msg["data"] = data;

    send_message(update_msg.dump());
}

void WebSocketSession::end_session()
{
    logger_->info("Session: {} disconnected. Cleaning up.", GetSessionID());
    {
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    subscribed_signals_.clear();
    }
    json disconnect_msg;
    disconnect_msg["type"] = "disconnect";
    disconnect_msg["session_id"] = GetSessionID();
    send_message(disconnect_msg.dump());
    server_.close_session(GetSessionID());
}

void WebSocketSession::on_disconnect()
{
    logger_->info("Session {}: Handling disconnection.", GetSessionID());
    end_session();
}

