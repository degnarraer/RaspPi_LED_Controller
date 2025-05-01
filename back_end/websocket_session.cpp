#include "websocket_session.h"
#include "websocket_server.h"

#define MAX_BATCH_COUNT 10
#define MAX_RETRY_ATTEMPTS 5

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

void WebSocketSession::send_message(const std::string& message)
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

        outgoing_messages_.push_back(message);
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

        // Move retryable messages back to outgoing if under retry limit
        for (auto it = retry_messages_.begin(); it != retry_messages_.end();)
        {
            if (it->retry_count < MAX_RETRY_ATTEMPTS)
            {
                ++it->retry_count;
                outgoing_messages_.push_front(it->message);
                it = retry_messages_.erase(it);
            }
            else
            {
                logger_->warn("Dropping message after {} retries: {}", MAX_RETRY_ATTEMPTS, it->message);
                it = retry_messages_.erase(it);  // Drop it
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
                            //response["message"] = "Was not subscribed to " + incoming["signal"].get<std::string>();
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
                case MessageType::Text:
                    logger_->info("Session: {} Text Message", GetSessionID());
                    break;
                case MessageType::Signal:
                    logger_->info("Session: {} Signal Message", GetSessionID());
                    break;
                case MessageType::Unknown:
                default:
                    logger_->warn("Session: {} Unknown Message", GetSessionID());
                    //json response;
                    //response["type"] = type_to_string_.at(MessageType::Echo);
                    //response["message"] = "Unknown message type";
                    //send_message(response.dump());
                    break;
            }
        }
        else
        {
            //json response;
            //response["type"] = type_to_string_.at(MessageType::Echo);
            //response["message"] = "Unsupported Message Format";
            //send_message(response.dump());
        }
    }
    catch (const std::exception& e)
    {
        logger_->warn("Session {}: Failed to parse JSON: {}", GetSessionID(), e.what());
        //json error;
        //error["type"] = "error";
        //error["message"] = "Invalid JSON";
        //send_message(error.dump());
    }
}

void WebSocketSession::do_write()
{
    std::lock_guard<std::mutex> lock(write_mutex_);
    json batch_message = json::array();  // Create a JSON array to store the messages.  
    if (!ws_.is_open())
    {
        logger_->warn("WebSocket not open.");
        server_.close_session(GetSessionID());
        writing_ = false;
        return;
    }

    if (outgoing_messages_.empty())
    {
        writing_ = false;
        logger_->debug("Message queue empty. Writing flag reset.");
        return;
    }

    size_t batch_count = std::min(static_cast<size_t>(MAX_BATCH_COUNT), outgoing_messages_.size());
    for (size_t i = 0; i < batch_count; ++i)
    {
        const std::string& message = outgoing_messages_.front();
        
        if (is_valid_utf8(message))
        {
            try
            {
                json msg_json = json::parse(message);
                batch_message.push_back(std::move(msg_json));
                outgoing_messages_.pop_front();  // Only pop on success
            }
            catch (const std::exception& e)
            {
                logger_->warn("Session {}: Skipped invalid JSON message: {}", session_id_, message);
            }
        }
        else
        {
            logger_->warn("Session {}: Skipped invalid UTF-8 message: {}", session_id_, message);
        }
    }

    if (batch_message.empty())
    {
        writing_ = false;
        logger_->debug("No valid messages to write.");
        return;
    }

    std::string combined_message = batch_message.dump();
    retry_messages_.push_back(RetryMessage(combined_message));
    ws_.text(true);
    ws_.async_write(
        asio::buffer(combined_message),
        beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this())
    );
}

void WebSocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if (ec)
    {
        handle_write_error(ec);
        return;
    }
    logger_->debug("Message sent successfully. Continuing to write if more messages exist.");
    remove_sent_message_from_retry_queue();
    do_write();
}

void WebSocketSession::handle_write_error(beast::error_code ec)
{
    if (ec)
    {
        logger_->warn("Write error: {}", ec.message());

        if (ec == beast::errc::not_connected || ec == beast::errc::connection_aborted)
        {
            logger_->warn("Connection lost or aborted. Closing session...");
            server_.close_session(GetSessionID());
            return;
        }

        else if (ec == beast::errc::network_unreachable || ec == beast::errc::host_unreachable)
        {
            logger_->warn("Network issue detected, retrying after backoff...");
            schedule_backoff();
            return;
        }

        else if (ec == beast::errc::connection_refused)
        {
            logger_->warn("Connection refused. Retrying...");
            schedule_backoff();
            return;
        }

        else
        {
            logger_->warn("Unexpected WebSocket error: {}. Retrying...", ec.message());
            schedule_backoff();
            return;
        }
    }
}

void WebSocketSession::remove_sent_message_from_retry_queue()
{
    std::lock_guard<std::mutex> retry_lock(retry_mutex_);

    if (!retry_messages_.empty())
    {
        retry_messages_.pop_front();
        logger_->debug("Successfully sent message and removed from retry queue.");
    }
}

bool WebSocketSession::subscribe_to_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);

    if (subscribed_signals_.find(signal_name) != subscribed_signals_.end()) {
        logger_->debug("Session: {} Already subscribed to signal: {}.", session_id_, signal_name);
        return false;
    }
    logger_->debug("Session: {} Subscribed to signal: {}.", session_id_, signal_name);
    subscribed_signals_.insert(signal_name);
    return true;
}

bool WebSocketSession::unsubscribe_from_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    if (subscribed_signals_.find(signal_name) != subscribed_signals_.end()) {        
        logger_->debug("Session: {} Unsubscribed from signal: {}.", session_id_, signal_name);
        subscribed_signals_.erase(signal_name);
        return true;
    }
    logger_->debug("Session: {} wasn't subscribed to signal: {}.", session_id_, signal_name);
    return false;
}

bool WebSocketSession::is_subscribed_to(const std::string& signal_name) const
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    return subscribed_signals_.count(signal_name) > 0;
}