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
    : ws_(std::move(socket)), server_(server), backoff_timer_(ws_.get_executor())
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
                self->server_.close_session(self->GetSessionID());
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
            }
        });
}

std::string WebSocketSession::GetSessionID() const
{
    return session_id_;
}

void WebSocketSession::send_message(const WebSocketMessage& webSocketMessage)
{
    bool should_write = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (outgoing_messages_.size() >= MAX_QUEUE_SIZE)
        {
            if (!backoff_enabled_)
            {
                backoff_enabled_ = true;
                backoff_attempts_++;
                logger_->warn("Backoff enabled for session {}, queue size {}", session_id_, outgoing_messages_.size());
                schedule_backoff();
            }
            return;
        }
        if (!is_valid_utf8(webSocketMessage.message))
        {
            logger_->warn("Session {}: Invalid UTF-8 message, skipping...", session_id_);
            return;
        }

        outgoing_messages_.push_back(webSocketMessage);
        should_write = !writing_ && !backoff_enabled_;
        if (should_write)
        {
            writing_ = true;
        }
    }

    if (should_write)
    {
        do_write();
    }
}

void WebSocketSession::send_binary_message(const std::vector<uint8_t>& message)
{
    WebSocketMessage binary_msg(message, MessagePriority::Low, false);
    send_message(binary_msg);
}

void WebSocketSession::schedule_backoff()
{
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
    bool should_write = false;

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::lock_guard<std::mutex> retry_lock(retry_mutex_);

        backoff_enabled_ = false;

        for (auto it = retry_messages_.begin(); it != retry_messages_.end();)
        {
            if (it->retry_count < MAX_RETRY_ATTEMPTS)
            {
                ++it->retry_count;
                outgoing_messages_.push_front(*it);
                it = retry_messages_.erase(it);
            }
            else
            {
                logger_->warn("Dropping message after {} retries: {}", MAX_RETRY_ATTEMPTS, it->message);
                it = retry_messages_.erase(it);
            }
        }

        if (!outgoing_messages_.empty())
        {
            writing_ = true;
            should_write = true;
        }
    }

    if (should_write)
    {
        do_write();
    }
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
                self->handle_read_error(ec);
                return;
            }
            self->on_read(bytes_transferred);
            self->do_read();
        });
}

void WebSocketSession::handle_read_error(beast::error_code ec)
{
    if (ec)
    {
        logger_->warn("Read error: {}", ec.message());

        // Handle permanent errors
        if (ec == beast::errc::not_connected || ec == beast::errc::connection_aborted)
        {
            logger_->warn("Connection lost or aborted. Closing session...");
            server_.close_session(GetSessionID());
            return;
        }

        // Handle temporary network-related errors
        else if (ec == beast::errc::network_unreachable || ec == beast::errc::host_unreachable)
        {
            logger_->warn("Network issue detected, retrying after backoff...");
            schedule_backoff();
            return;
        }

        // Handle other transient errors (e.g., connection refused)
        else if (ec == beast::errc::connection_refused)
        {
            logger_->warn("Connection refused. Retrying...");
            schedule_backoff();
            return;
        }

        // For any other unhandled errors, log and schedule backoff
        else
        {
            logger_->warn("Unexpected WebSocket read error: {}. Retrying...", ec.message());
            schedule_backoff();
            return;
        }
    }
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
                    logger_->info("Session: {} Subscribe Message", GetSessionID());
                    if(incoming.contains("signal"))
                    {
                        if(subscribe_to_signal(incoming["signal"]))
                        {
                            //json response;
                            //response["type"] = type_to_string_.at(MessageType::Echo);
                            //response["message"] = "Successfully subscribed to " + incoming["signal"].get<std::string>();
                            //send_message(response.dump());
                        }
                        else
                        {
                            //json response;
                            //response["type"] = type_to_string_.at(MessageType::Echo);
                            //response["message"] = "Already subscribed to " + incoming["signal"].get<std::string>();
                            //send_message(response.dump());
                        }
                    }
                    else
                    {
                        logger_->warn("Session: {} Subscribe Message without signal", GetSessionID());
                        //json response;
                        //response["type"] = type_to_string_.at(MessageType::Echo);
                        //response["message"] = "Subscribe message missing signal";
                        //send_message(response.dump());
                    }
                    break;
                case MessageType::Unsubscribe:
                    logger_->info("Session: {} Unsubscribe Message", GetSessionID());
                    if(incoming.contains("signal"))
                    {
                        
                        if(unsubscribe_from_signal(incoming["signal"]))
                        {
                            //json response;
                            //response["type"] = type_to_string_.at(MessageType::Echo);
                            //response["message"] = "Successfully unsubscribed from " + incoming["signal"].get<std::string>();
                            //send_message(response.dump());
                        }
                        else
                        {
                            //json response;
                            //response["type"] = type_to_string_.at(MessageType::Echo);
                            //response["message"] = "Already unsubscribed from " + incoming["signal"].get<std::string>();
                            //send_message(response.dump());
                        }
                    }
                    else
                    {
                        logger_->warn("Session: {} Unsubscribe Message without signal", GetSessionID());
                        //json response;
                        //response["type"] = type_to_string_.at(MessageType::Echo);
                        //response["message"] = "Unsubscribe message missing signal";
                        //send_message(response.dump());
                    }
                    break;
                case MessageType::Signal:
                {
                    std::string signal_data = incoming["signal"].get<std::string>();
                    logger_->info("Received signal data: {}", signal_data);
                    // Handle signal data processing
                    break;
                }
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

void WebSocketSession::do_write()
{
    std::lock_guard<std::mutex> lock(write_mutex_);

    if (outgoing_messages_.empty())
    {
        writing_ = false;
        return;
    }

    auto webSocketMessage = outgoing_messages_.front();
    outgoing_messages_.pop_front();
    writing_ = true;

    switch(webSocketMessage.webSocket_Message_type)
    {
        case WebSocketMessageType::Text:
            {
                ws_.text(true);
                ws_.async_write(
                    asio::buffer(webSocketMessage.message),
                    [self = shared_from_this(), webSocketMessage](beast::error_code ec, std::size_t bytes_transferred)
                    {
                        if (ec)
                        {
                            self->rate_limited_log->log("write error", spdlog::level::warn, "Text write error: {}", ec.message());
                            self->schedule_backoff();
                            self->retry_message(webSocketMessage);
                        }
                        else
                        {
                            self->logger_->debug("Sent text message: {}, bytes: {}", webSocketMessage.message, bytes_transferred);
                            self->do_write();
                        }
                    });
            }
            break;
        case WebSocketMessageType::Binary:
            {
                ws_.binary(true);
                ws_.async_write(
                    asio::buffer(webSocketMessage.binary_data),
                    [self = shared_from_this(), webSocketMessage](beast::error_code ec, std::size_t bytes_transferred)
                    {
                        if (ec)
                        {
                            self->rate_limited_log->log("write error", spdlog::level::warn, "Binary write error: {}", ec.message());
                            self->schedule_backoff();
                            self->retry_message(webSocketMessage);
                        }
                        else
                        {
                            self->logger_->debug("Sent binary message, bytes: {}", bytes_transferred);
                            self->do_write();
                        }
                    });
            }
            break;
        default:
            logger_->warn("Session: {} Unknown message type", GetSessionID());
            writing_ = false;
            return;
    }
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
        subscribed_signals_.insert(signal_name);
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

void WebSocketSession::handle_disconnection()
{
    logger_->info("Session: {} disconnected. Cleaning up.", GetSessionID());

    std::lock_guard<std::mutex> lock(subscription_mutex_);
    subscribed_signals_.clear();

    // Optionally, send a disconnect message or cleanup resources as needed.
    // This is just an example:
    json disconnect_msg;
    disconnect_msg["type"] = "disconnect";
    disconnect_msg["session_id"] = GetSessionID();
    send_message(disconnect_msg.dump());
}

void WebSocketSession::on_disconnect()
{
    logger_->info("Session {}: Handling disconnection.", GetSessionID());
    handle_disconnection();
    server_.close_session(GetSessionID());
}

