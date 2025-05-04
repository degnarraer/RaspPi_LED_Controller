#include "websocket_server.h"

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

void WebSocketServer::broadcast_message_to_websocket(const WebSocketMessage& webSocketMessage)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->debug("Broadcast Message to Clients: {}.", webSocketMessage.message);
    for (auto& [id, session] : sessions_)
    {
        if (auto shared_session = session.lock())
        {
            shared_session->send_message(webSocketMessage);
        }
    }
}

void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& message)
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
            session->send_message(WebSocketMessage("Session closing...", MessagePriority::High, true));
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
            shared_session->send_message(WebSocketMessage("Server shutting down...", MessagePriority::High, true));
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