#include "websocket_server.h"
const size_t MAX_QUEUE_SIZE = 500;

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
        backoff_enabled_ = false;
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
    // Simple check for valid UTF-8 encoding
    // More advanced checks may be needed based on your use case

    size_t i = 0;
    size_t len = str.length();
    while (i < len)
    {
        unsigned char c = str[i];
        
        // ASCII (1 byte)
        if (c < 0x80)
        {
            ++i;
        }
        // 2-byte UTF-8
        else if ((c & 0xE0) == 0xC0)
        {
            if (i + 1 < len && (str[i + 1] & 0xC0) == 0x80)
            {
                i += 2;
            }
            else
            {
                return false;
            }
        }
        // 3-byte UTF-8
        else if ((c & 0xF0) == 0xE0)
        {
            if (i + 2 < len && (str[i + 1] & 0xC0) == 0x80 && (str[i + 2] & 0xC0) == 0x80)
            {
                i += 3;
            }
            else
            {
                return false;
            }
        }
        // 4-byte UTF-8
        else if ((c & 0xF8) == 0xF0)
        {
            if (i + 3 < len && (str[i + 1] & 0xC0) == 0x80 && (str[i + 2] & 0xC0) == 0x80 && (str[i + 3] & 0xC0) == 0x80)
            {
                i += 4;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

void WebSocketSession::do_read()
{
    logger_->debug("Session: {} Do Read.", GetSessionID());

    ws_.async_read(buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                self->logger_->warn("Session {}: Read error: {}", self->GetSessionID(), ec.message());
                self->server_.close_session(self->GetSessionID());
                return;
            }
            self->on_read(bytes_transferred);
            self->do_read();
        });
}

void WebSocketSession::on_read(std::size_t)
{
    logger_->debug("Session: {} On Read.", GetSessionID());
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
                        subscribe_to_signal(incoming["signal"]);
                        json response;
                        response["type"] = type_to_string_.at(MessageType::Echo);
                        response["message"] = "Successfully subscribed to " + incoming["signal"].get<std::string>();
                        send_message(response.dump());
                    }
                    else
                    {
                        logger_->warn("Session: {} Subscribe Message without signal", GetSessionID());
                        json response;
                        response["type"] = type_to_string_.at(MessageType::Echo);
                        response["message"] = "Subscribe message missing signal";
                        send_message(response.dump());
                    }
                    break;
                case MessageType::Unsubscribe:
                    logger_->info("Session: {} Unsubscribe Message", GetSessionID());
                    if(incoming.contains("signal"))
                    {
                        unsubscribe_from_signal(incoming["signal"]);
                        json response;
                        response["type"] = type_to_string_.at(MessageType::Echo);
                        response["message"] = "Successfully unsubscribed from " + incoming["signal"].get<std::string>();
                        send_message(response.dump());
                    }
                    else
                    {
                        logger_->warn("Session: {} Unsubscribe Message without signal", GetSessionID());
                        json response;
                        response["type"] = type_to_string_.at(MessageType::Echo);
                        response["message"] = "Unsubscribe message missing signal";
                        send_message(response.dump());
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
                    json response;
                    response["type"] = type_to_string_.at(MessageType::Echo);
                    response["message"] = "Unknown message type";
                    send_message(response.dump());
                    break;
            }
        }
        else
        {
            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = "Unsupported Message Format";
            send_message(response.dump());
        }
    }
    catch (const std::exception& e)
    {
        logger_->warn("Session {}: Failed to parse JSON: {}", GetSessionID(), e.what());
        json error;
        error["type"] = "error";
        error["message"] = "Invalid JSON";
        send_message(error.dump());
    }
}


void WebSocketSession::do_write()
{
    std::string message;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        
        // If WebSocket is not open, stop the writing process and close the session.
        if (!ws_.is_open())
        {
            logger_->warn("WebSocket not open.");
            server_.close_session(GetSessionID());
            writing_ = false; // Reset writing flag if WebSocket isn't open.
            return;
        }

        // If no more messages to send, reset writing flag and return.
        if (outgoing_messages_.empty())
        {
            writing_ = false;
            logger_->debug("Message queue empty. Writing flag reset.");
            return;
        }

        // Pop the next message from the queue.
        message = std::move(outgoing_messages_.front());
        outgoing_messages_.pop_front();
    }

    // Set WebSocket to text mode based on UTF-8 validity.
    ws_.text(is_valid_utf8(message));

    // Start asynchronous write operation and handle response via `on_write`.
    ws_.async_write(
        asio::buffer(message),
        beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this())
    );
}

void WebSocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if (ec)
    {
        logger_->warn("Write error: {}", ec.message());
        writing_ = false;
        server_.close_session(GetSessionID());
        return;
    }
    logger_->debug("Message sent successfully. Continuing to write if more messages exist.");
    do_write();
}

void WebSocketSession::subscribe_to_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    logger_->debug("Session: {} Subscribed to signal: {}.", session_id_, signal_name);
    subscribed_signals_.insert(signal_name);
}

void WebSocketSession::unsubscribe_from_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    logger_->debug("Session: {} Unsubscribed to signal: {}.", session_id_, signal_name);
    subscribed_signals_.erase(signal_name);
}

bool WebSocketSession::is_subscribed_to(const std::string& signal_name) const
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    return subscribed_signals_.count(signal_name) > 0;
}

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

WebSocketServer::WebSocketServer(short port)
    : ioc_()
    , port_(port)
    , acceptor_(ioc_, tcp::endpoint(tcp::v4(), port_))
{
    logger_ = spdlog::stdout_color_mt("websocket_server");
    logger_->set_level(spdlog::level::info);

    try {        
        unsigned short server_port = acceptor_.local_endpoint().port();
        logger_->info("WebSocket Server is running on {}", server_port);
    } catch (const std::exception& e) {
        logger_->error("Error retrieving port: {}", e.what());
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.set_option(boost::asio::socket_base::keep_alive(true));
    acceptor_.set_option(boost::asio::socket_base::receive_buffer_size(8192));
    acceptor_.set_option(boost::asio::socket_base::send_buffer_size(8192));
    logger_->debug("Instantiated WebSocketServer.");
}

WebSocketServer::~WebSocketServer()
{
    logger_->debug("Destroying WebSocketServer.");
    close_all_sessions();
    Stop();
}

void WebSocketServer::Run()
{
    logger_->info("Run.");

    if (!acceptor_.is_open())
    {
        logger_->error("Acceptor is not open!");
        return;
    }

    boost::system::error_code ec;
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        logger_->error("Error starting to listen: {}", ec.message());
        return;
    }
    logger_->info("Acceptor is listening...");

    do_accept();

    if (!ioc_thread_.joinable())
    {
        ioc_thread_ = std::thread([this]()
        {
            logger_->info("I/O Context thread started.");
            ioc_.run();
        });
    }
}

void WebSocketServer::Stop()
{
    logger_->info("Stop.");
    if (acceptor_.is_open())
    {
        acceptor_.close();
        logger_->info("acceptor is closed.");
    }
    ioc_.stop();
    logger_->info("I/O Context thread stopped.");
    if (ioc_thread_.joinable())
    {
        ioc_thread_.join();
        logger_->info("I/O Context thread joined.");
    }
}

void WebSocketServer::broadcast_message_to_websocket(const std::string& message)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->debug("Broadcast Message to Clients: {}.", message);
    for (auto& [id, session] : sessions_)
    {
        if (auto shared_session = session.lock())
        {
            shared_session->send_message(message);
        }
    }
}

void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const std::string& message)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (auto session = it->second.lock())
        {
            if (session->is_subscribed_to(signal_name))
            {
                session->send_message(message);
            }
            ++it;
        }
        else
        {
            it = sessions_.erase(it);
        }
    }
}

void WebSocketServer::register_session(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Registering Session {}.", session->GetSessionID());
    sessions_[session->GetSessionID()] = session;
    logger_->info("Session {}: Registered.", session->GetSessionID());
}

void WebSocketServer::register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Registering Backend Client {}.", client->GetName());
    backend_clients_[client->GetName()] = client;
    logger_->info("Backend Client {}: Registered.", client->GetName());
}

void WebSocketServer::close_session(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Closing session.");
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        if (auto session = it->second.lock())
        {
            session->send_message("Session closing...");
            session->close();
            logger_->info("Session {}: Closed.", session_id);
        }
        sessions_.erase(it);
    }
    else
    {
        logger_->warn("Attempted to close unknown session: {}.", session_id);
    }
}

void WebSocketServer::close_all_sessions()
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Closing all sessions.");
    for (auto& [id, session] : sessions_)
    {
        if (auto shared_session = session.lock())
        {
            shared_session->send_message("Server shutting down...");
            shared_session->close();
        }
    }
    sessions_.clear();
    logger_->info("All sessions closed.");
}

void WebSocketServer::do_accept()
{
    logger_->info("Waiting for incoming connection...");
    auto self = shared_from_this();
    acceptor_.async_accept(
        [self](beast::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                auto remote_endpoint = socket.remote_endpoint();
                std::string remote_ip = remote_endpoint.address().to_string();
                unsigned short remote_port = remote_endpoint.port();
                self->logger_->info("Incoming WebSocket session from {}:{}", remote_ip, remote_port);
                auto session = std::make_shared<WebSocketSession>(std::move(socket), *self);
                self->register_session(session);
                session->run();
            }
            else if (ec == asio::error::operation_aborted)
            {
                self->logger_->warn("Accept operation aborted.");
                return;
            }
            else
            {
                self->logger_->error("Connection accept error: {}.", ec.message());
            }
            self->do_accept();
        });
}