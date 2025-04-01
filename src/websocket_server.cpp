#include "websocket_server.h"

WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket)), server_(server)
{
    logger_  = InitializeLogger("Web Socket Session", spdlog::level::debug);
    session_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
    logger_->info("Created new web socket session: {}", session_id_);
}

void WebSocketSession::run()
{
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
            }
        });
}

void WebSocketSession::close()
{
    ws_.close(websocket::close_code::normal);
}

std::string WebSocketSession::GetSessionID() const
{
    return session_id_;
}

void WebSocketSession::send_message(const std::string& message)
{
    logger_->debug("Session: {} Send Message to Cliets: {}.", GetSessionID(), message);
    ws_.text(ws_.got_text());
    ws_.async_write(boost::asio::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t)
        {
            if (ec)
            {
                self->logger_->error("Error sending message: {}", ec.message());
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
                self->logger_->warn("Read error: {}", ec.message());
                self->server_.close_session(self->GetSessionID());
                return;
            }
            self->on_read(bytes_transferred);
        });
}

void WebSocketSession::on_read(std::size_t)
{
    logger_->debug("Session: {} On Read.", GetSessionID());
    auto message = beast::buffers_to_string(buffer_.data());
    logger_->trace("Received: {}", message);
    buffer_.consume(buffer_.size());
    do_write("Echo: " + message);
}

void WebSocketSession::do_write(const std::string& message)
{
    logger_->debug("do write: {}", message);
    ws_.text(ws_.got_text());
    ws_.async_write(boost::asio::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t)
        {
            if (!ec)
            {
                self->do_read();
            }
            else 
            {
                self->logger_->warn("Write error: {}", ec.message());
            }
        });
}

WebSocketServer::WebSocketServer(short port)
    : port_(port)
    , ioc_()
    , acceptor_(ioc_, tcp::endpoint(tcp::v4(), port_))
{
    logger_ = InitializeLogger("Web Socket Server", spdlog::level::info);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    logger_->debug("Instantiate");
}

WebSocketServer::~WebSocketServer()
{
    logger_->debug("Destroy");
    Stop();
}

void WebSocketServer::Run()
{
    logger_->debug("Run.");
    if (!acceptor_.is_open())
    {
        acceptor_.open(tcp::v4());
        logger_->info("acceptor is opened.");
    }

    if (!ioc_thread_.joinable())
    {
        //ioc_.restart();
        ioc_thread_ = std::thread([this]()
        {
            logger_->info("I/O Context thread started.");
            ioc_.run();
        });
    }
    do_accept();
}

void WebSocketServer::Stop()
{
    logger_->debug("Stop.");
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
    logger_->debug("Closing session.");
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        if (auto session = it->second.lock())
        {
            session->send_message("Session closing...");
            session->close();
            logger_->info("Session {} closed.", session_id);
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
    logger_->debug("Registering session {}.", session->GetSessionID());
    sessions_[session->GetSessionID()] = session;
    logger_->info("Session {} registered.", session->GetSessionID());
}

void WebSocketServer::do_accept()
{
    logger_->debug("Do accept");
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                logger_->info("Incomming Session.");
                auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
                register_session(session);
                session->run();
            }
            else
            {
                logger_->warn("Connection accept error: {}.", ec.message());
            }
            do_accept();
        });
}