#include "websocket_server.h"
const size_t MAX_QUEUE_SIZE = 500;

WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket)), server_(server)
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
    asio::post(ws_.get_executor(),
        [self = shared_from_this(), message]()
        {
            bool start_write = false;

            {
                std::lock_guard<std::mutex> lock(self->write_mutex_);

                if (self->outgoing_messages_.size() >= MAX_QUEUE_SIZE)
                {
                    self->outgoing_messages_.pop_front();
                    self->logger_->warn("Session {}: Dropped Message.", self->GetSessionID());
                }

                self->outgoing_messages_.push_back(message);

                if (!self->writing_)
                {
                    self->writing_ = true;
                    start_write = true;
                }
            }

            if (start_write)
            {
                self->do_write();
            }
        });
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
                    }
                    else
                    {
                        logger_->warn("Session: {} Subscribe Message without signal", GetSessionID());
                    }
                    break;
                case MessageType::Unsubscribe:
                    logger_->info("Session: {} Unsubscribe Message", GetSessionID());
                    if(incoming.contains("signal"))
                    {
                        unsubscribe_from_signal(incoming["signal"]);
                    }
                    else
                    {
                        logger_->warn("Session: {} Unsubscribe Message without signal", GetSessionID());
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
                    break;
            }

            json response;
            response["type"] = type_to_string_.at(MessageType::Echo);
            response["message"] = incoming["message"];
            send_message(response.dump());
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
    logger_->debug("Session: {} Do Write.", GetSessionID());

    if (!ws_.is_open())
    {
        logger_->warn("WebSocket not open.");
        server_.close_session(GetSessionID());
        return;
    }

    std::string message;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (outgoing_messages_.empty())
        {
            writing_ = false;
            logger_->debug("Message queue empty. Writing flag reset.");
            return;
        }

        message = std::move(outgoing_messages_.front());
        outgoing_messages_.pop_front();
    }

    ws_.text(ws_.got_text());
    ws_.async_write(asio::buffer(message),
        beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
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
    logger_->debug("Broadcast Message to Cliets: {}.", message);
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

void WebSocketServer::register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client)
{
    logger_->debug("Registering backend client: {}.", client->GetName());
    if (!client)
    {
        logger_->warn("Attempted to register a null backend client.");
        return;
    }
    backend_clients_[client->GetName()] = client;
    logger_->info("Registered backend client: {}.", client->GetName());
}

void WebSocketServer::deregister_backend_client(const std::string& client_name)
{
    logger_->debug("Deregistering backend client: {}.", client_name);
    if (backend_clients_.erase(client_name))
    {
        logger_->info("Deregistered backend client: {}.", client_name);
    }
    else
    {
        logger_->warn("Attempted to deregister unknown backend client: {}.", client_name);
    }
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

void WebSocketServer::register_session(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Registering Session {}.", session->GetSessionID());
    sessions_[session->GetSessionID()] = session;
    logger_->info("Session {}: Registered.", session->GetSessionID());
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