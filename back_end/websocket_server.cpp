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
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    logger_->info("Closing all sessions.");

    for (auto& [session_id, session] : sessions_)
    {
        if (session && session->isRunning())
        {
            logger_->info("Closing session: {}", session_id);
            session->close();
        }
    }

    sessions_.clear();
    logger_->info("All sessions closed.");
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

bool WebSocketServer::registerSession(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    try
    {
        const std::string& session_id = session->getSessionID();
        if(session_id.empty())
        {
            logger_->error("Session ID is empty, cannot register session.");
            return false;
        }
        sessions_[session_id] = session;
        logger_->info("Session \"{}\" Registered", session_id);
        return true;
    }
    catch (const std::exception& e)
    {
        logger_->error("Failed to register session: {}", e.what());
        return false;
    }
}

bool WebSocketServer::unregisterSession(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    try
    {
        const std::string& session_id = session->getSessionID();
        sessions_.erase(session_id);
        logger_->info("Session \"{}\" UnRegistered", session_id);
        return true;
    }
    catch(const std::exception& e)
    {
        logger_->error("Failed to unregister session: {}", e.what());
        return false;
    }
}

void WebSocketServer::broadcast(const WebSocketMessage& webSocketMessage)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    logger_->debug("Broadcast message \"{}\" to WebSocket.", webSocketMessage.message);
    for (auto& [id, session] : sessions_)
    {
        if (session && session->isRunning())
        {
            session->sendMessage(webSocketMessage);
        }
    }
}

// Optimized broadcast to subscribers of a signal
void WebSocketServer::broadcast_signal_to_websocket(const std::string& signal_name, const WebSocketMessage& webSocketMessage)
{
    std::unordered_set<std::string> subscribers_copy;
    {
        std::lock_guard<std::mutex> lock(signal_subscriptions_mutex_);
        auto it = signal_subscriptions_.find(signal_name);
        if (it != signal_subscriptions_.end())
        {
            subscribers_copy = it->second; // copy so we can unlock early
        }
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    logger_->debug("Broadcast signal \"{}\" to WebSocket.", signal_name);

    for (const auto& session_id : subscribers_copy)
    {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end())
        {
            auto& session = it->second;
            if (session && session->isRunning())
            {
                session->sendMessage(webSocketMessage);
            }
        }
    }
}

 void WebSocketServer::subscribe_session_to_signal(const std::string& session_id, const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(signal_subscriptions_mutex_);
    signal_subscriptions_[signal_name].insert(session_id);
    logger_->info("Session {} subscribed to signal {}", session_id, signal_name);
}

// Call this to unregister a subscription for a session
void WebSocketServer::unsubscribe_session_from_signal(const std::string& session_id, const std::string& signal_name)
{
    std::lock_guard<std::mutex> lock(signal_subscriptions_mutex_);
    auto it = signal_subscriptions_.find(signal_name);
    if (it != signal_subscriptions_.end())
    {
        it->second.erase(session_id);
        if (it->second.empty())
        {
            signal_subscriptions_.erase(it);
        }
        logger_->info("Session {} unsubscribed from signal {}", session_id, signal_name);
    }
}

// Unsubscribe session from all signals (call on session end)
void WebSocketServer::unsubscribe_session_from_all_signals(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(signal_subscriptions_mutex_);
    for (auto it = signal_subscriptions_.begin(); it != signal_subscriptions_.end(); )
    {
        it->second.erase(session_id);
        if (it->second.empty())
            it = signal_subscriptions_.erase(it);
        else
            ++it;
    }
    logger_->info("Session {} unsubscribed from all signals", session_id);
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
                if(self->registerSession(session))
                {
                    session->start();
                }
                else
                {
                    self->logger_->error("Failed to register session.");
                    session->close();
                }
            }

            if (self->running_)
                self->do_accept();
        });
}

