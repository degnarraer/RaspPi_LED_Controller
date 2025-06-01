#include "websocket_server.h"


WebSocketServer::WebSocketServer(unsigned short port, unsigned int thread_count)
    : ioc_(1)
    , acceptor_(ioc_)
    , thread_count_(thread_count == 0 ? std::max(1u, std::thread::hardware_concurrency()) : thread_count)
    , logger_(initializeLogger("WebSocketServer", spdlog::level::info))
{
    beast::error_code ec;

    tcp::endpoint endpoint(tcp::v4(), port);

    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
        logger_->error("Open failed: {}", ec.message());
        return;
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec)
    {
        logger_->error("Set option failed: {}", ec.message());
        return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        logger_->error("Bind failed: {}", ec.message());
        return;
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec)
    {
        logger_->error("Listen failed: {}", ec.message());
        return;
    }
}

void WebSocketServer::start()
{
    if (running_)
        return;

    running_ = true;

    do_accept();

    for (unsigned int i = 0; i < thread_count_; ++i)
    {
        thread_pool_.emplace_back([this]()
        {
            ioc_.run();
        });
    }
}

void WebSocketServer::stop()
{
    if (!running_)
        return;

    running_ = false;

    ioc_.stop();

    for (auto& t : thread_pool_)
    {
        if (t.joinable())
            t.join();
    }
    thread_pool_.clear();
}

void WebSocketServer::close_session(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    logger_->info("Closing session: {}", session_id);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        auto& session = it->second;
        if (session->isRunning())
        {
            session->close();
        }
        else
        {
            logger_->warn("Session is not running: {}", session_id);
        }
    }
    else
    {
        logger_->warn("Attempted to close unknown session: {}.", session_id);
    }
}

void WebSocketServer::close_all_sessions()
{

}

void WebSocketServer::end_session(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    logger_->info("Ending session: {}", session_id);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        auto& session = it->second;
        if (session->isRunning())
        {
            session->close();
        }
        sessions_.erase(it);
        logger_->info("Session ended: {}", session_id);
    }
    else
    {
        logger_->warn("Attempted to end unknown session: {}.", session_id);
    }
}

void WebSocketServer::register_notification_client(const std::string& client_name, WebSocketServerNotificationClient* client)
{
    std::lock_guard<std::mutex> lock(notification_clients_mutex_);
    logger_->info("Registering Notification Client {}.", client_name);
    notification_clients_[client_name] = client;
    logger_->info("Notification Client {}: Registered.", client_name);
}

void WebSocketServer::unregister_notification_client(const std::string& client_name)
{
    std::lock_guard<std::mutex> lock(notification_clients_mutex_);
    auto it = notification_clients_.find(client_name);
    if (it != notification_clients_.end())
    {
        logger_->info("Unregistering Notification Client {}.", client_name);
        notification_clients_.erase(it);
        logger_->info("Notification Client {}: Unregistered.", client_name);
    }
    else
    {
        logger_->warn("Attempted to unregister unknown Notification Client: {}.", client_name);
    }
}

void WebSocketServer::registerSession(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    const std::string& session_id = session->getSessionID();
    sessions_[session_id] = session;
}

void WebSocketServer::unregisterSession(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    const std::string& session_id = session->getSessionID();
    sessions_.erase(session_id);
}

void WebSocketServer::broadcast(const std::string& message)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_)
    {
        if (session && session->isRunning())
        {
            session->sendMessage(message);
        }
    }
}

void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& webSocketMessage)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_)
    {
        if (session && session->isSubscribedToSignal(signal_name))
        {
            session->sendMessage(webSocketMessage.message);
        }
    }
}

void WebSocketServer::do_accept()
{
    acceptor_.async_accept(
        [self = shared_from_this()](beast::error_code ec, tcp::socket socket)
        {
            if (ec)
            {
                self->logger_->error("Accept failed: {}", ec.message());
            }
            else
            {
                auto session = std::make_shared<WebSocketSession>(std::move(socket), self);
                session->start();
            }

            if (self->running_)
                self->do_accept();
        });
}

/*
void WebSocketServer::run()
{
    std::lock_guard<std::mutex> lock(server_mutex_);
    if (is_running_.exchange(true))
    {
        logger_->warn("WebSocketServer already running.");
        return;
    }

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
    ioc_.restart();
    do_accept();

    if (!ioc_thread_.joinable())
    {
        ioc_thread_ = std::thread([self = this]()
        {
            self->logger_->info("I/O Context thread started.");
            self->ioc_.run();
        });
    }
}

void WebSocketServer::stop()
{
    std::lock_guard<std::mutex> lock(server_mutex_);
    if (!is_running_.exchange(false))
    {
        logger_->warn("WebSocketServer is not running.");
        return;
    }

    logger_->info("Stop.");

    boost::system::error_code ec;
    if (acceptor_.is_open())
    {
        acceptor_.close(ec);
        if (ec)
        {
            logger_->error("Error closing acceptor: {}", ec.message());
        }
        else
        {
            logger_->info("Acceptor is closed.");
        }
    }

    ioc_.stop();
    logger_->info("I/O Context stopped.");

    if (ioc_thread_.joinable())
    {
        ioc_thread_.join();
        logger_->info("I/O Context thread joined.");
    }
}

void WebSocketServer::broadcast_message_to_websocket(const WebSocketMessage& webSocketMessage)
{
    std::vector<std::shared_ptr<WebSocketSession>> targets;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (auto session = it->second.lock())
            {
                targets.push_back(session);
                ++it;
            }
            else
            {
                it = sessions_.erase(it);
            }
        }
    }

    logger_->debug("Broadcasting message to sessions: {}", targets.size());
    for (auto& session : targets)
    {
        session->sendMessage(webSocketMessage);
    }
}

void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& message)
{
    std::vector<std::shared_ptr<WebSocketSession>> targets;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (auto session = it->second.lock())
            {
                //TO DO if (session->isSubscribedToSignal(signal_name))
                {
                    targets.push_back(session);
                }
                ++it;
            }
            else
            {
                it = sessions_.erase(it);
            }
        }
    }

    for (auto& session : targets)
    {
        session->sendMessage(message);
    }
}

void WebSocketServer::register_session(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Registering session: {}.", session->getSessionID());
    sessions_[session->getSessionID()] = session;
    logger_->info("Session registered: {}", session->getSessionID());
}

/*
void WebSocketServer::register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Registering Backend Client {}.", client->getName());
    backend_clients_[client->getName()] = client;
    logger_->info("Backend Client {}: Registered.", client->getName());
}
*/
/*
void WebSocketServer::close_session(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Closing session: {}", session_id);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        if (auto session = it->second.lock())
        {
            if(session->isRunning())
            {
                session->close();
            }
            else
            {
                logger_->warn("Session is not running: {}", session_id);
            }
        }
    }
    else
    {
        logger_->warn("Attempted to close unknown session: {}.", session_id);
    }
}

void WebSocketServer::end_session(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mutex_);
    logger_->info("Ending session: {}", session_id);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        if (auto session = it->second.lock())
        {
            if(session->isRunning())
            {
                session->close();
            }
        }
        sessions_.erase(it);
        logger_->info("Session ended: {}", session_id);
    }
    else
    {
        logger_->warn("Attempted to end unknown session: {}.", session_id);
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
            shared_session->sendMessage(WebSocketMessage("Server shutting down...", MessagePriority::High, true));
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
            if (!self->acceptor_.is_open())
            {
                self->logger_->info("Acceptor is closed. No longer accepting connections.");
                return;
            }
            self->do_accept();
        });
}
*/