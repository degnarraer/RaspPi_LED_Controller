#include "websocket_server.h"
#include <iostream>
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
        if (incoming.contains("type") && incoming.contains("message"))
        {
            logger_->trace("Message type: {}", incoming["type"].get<std::string>());

            switch (string_to_type_.at(incoming["type"]))
            {
                case MessageType::Subscribe:
                    logger_->info("Session: {} Subscribe Message", GetSessionID());
                    subscribe_to_signal(incoming["signal"]);
                    break;
                case MessageType::Unsubscribe:
                    logger_->info("Session: {} Unsubscribe Message", GetSessionID());
                    unsubscribe_from_signal(incoming["signal"]);
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
    subscribed_signals_.insert(signal_name);
}

void WebSocketSession::unsubscribe_from_signal(const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(subscription_mutex_);
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
    , acceptor_(ioc_)
{
    try
    {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), port_);
        acceptor_.open(endpoint.protocol());

        acceptor_.set_option(boost::asio::ip::v6_only(false));
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));

        acceptor_.bind(endpoint);
        acceptor_.listen();

        logger_ = spdlog::get("WebSocket Server");
        if (!logger_)
        {
            logger_ = spdlog::stdout_color_mt("WebSocket Server");
        }
        logger_->set_level(spdlog::level::info);

        auto bound_endpoint = acceptor_.local_endpoint();
        logger_->info("WebSocket Server is running on {}:{}.",
                      bound_endpoint.address().to_string(), bound_endpoint.port());
    }
    catch (const std::exception& e)
    {
        if (logger_)
            logger_->error("Error setting up WebSocket server: {}", e.what());
        else
            std::cerr << "Error setting up WebSocket server: " << e.what() << std::endl;
        throw;
    }

    try
    {
        boost::asio::ip::tcp::resolver resolver(ioc_);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");

        for (auto it = resolver.resolve(query); it != boost::asio::ip::tcp::resolver::iterator(); ++it)
        {
            auto addr = it->endpoint().address();

            if (!addr.is_loopback() && !addr.is_unspecified())
            {
                if (addr.is_v4())
                {
                    logger_->info("Detected LAN IPv4: {}", addr.to_string());
                }
                else if (addr.is_v6())
                {
                    if (!addr.to_string().rfind("fe80", 0) == 0)
                    {
                        logger_->info("Detected LAN IPv6: {}", addr.to_string());
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        logger_->error("Error detecting LAN IPs: {}", e.what());
    }

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
        beast::error_code ec;
        acceptor_.open(tcp::v6(), ec);
        if (ec)
        {
            logger_->error("Error opening acceptor: {}", ec.message());
            return;
        }
    }
    if (!ioc_thread_.joinable())
    {
        try
        {
            ioc_thread_ = std::thread([this]()
            {
                logger_->info("I/O Context thread started.");
                ioc_.run();
            });
        }
        catch (const std::exception& e)
        {
            logger_->error("Failed to start I/O context thread: {}", e.what());
        }
    }
    do_accept();
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
    logger_->debug("Broadcast to clients: Message: \"{}\".", message);
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sessions_copy = sessions_;
    }
    for (auto& [id, session] : sessions_copy)
    {
        if (auto shared_session = session.lock())
        {
            shared_session->send_message(message);
        }
    }
}

void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const std::string& message)
{
    logger_->debug("Broadcast to clients: Signal: \"{}\" Message: \"{}\".", signal_name, message);
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sessions_copy = sessions_;
    }
    for (auto it = sessions_copy.begin(); it != sessions_copy.end();)
    {
        if (auto session = it->second.lock())
        {
            if (session->is_subscribed_to(signal_name))
            {
                session->send_message(message);
            }
            ++it;
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
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket)
        {
            logger_->info("Incoming WebSocket session...");
            if (!ec)
            {
                auto remote_endpoint = socket.remote_endpoint();
                std::string remote_ip = remote_endpoint.address().to_string();
                unsigned short remote_port = remote_endpoint.port();
                logger_->info("From {}:{}", remote_ip, remote_port);
                auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
                register_session(session);
                session->run();
            }
            else
            {
                logger_->error("Connection accept error: {}.", ec.message());
            }
            do_accept();
        });
        logger_->info("Waiting for incoming connection...");
}