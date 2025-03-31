#include "websocket_server.h"

WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket)), server_(server)
{
    session_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
    logger_ = spdlog::get("WebSocketSessionLogger");
    if (!logger_)
    {
        logger_ = spdlog::stdout_color_mt("WebSocketSessionLogger");
    }
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
    auto message = beast::buffers_to_string(buffer_.data());
    logger_->info("Received: {}", message);
    buffer_.consume(buffer_.size());
    do_write("Echo: " + message);
}

void WebSocketSession::do_write(const std::string& message)
{
    logger_->info("Send: {}", message);
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
    : port_(port), acceptor_(ioc_, tcp::endpoint(tcp::v4(), port_))
{
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    logger_ = InitializeLogger("Web Socket Server", spdlog::level::info);
}

WebSocketServer::~WebSocketServer()
{
    Stop();
}

void WebSocketServer::Run()
{
    if (!acceptor_.is_open())
    {
        acceptor_.open(tcp::v4());
        logger_->info("WebSocket server acceptor is opened.");
    }

    if (!ioc_thread_.joinable())
    {
        ioc_.restart();
        ioc_thread_ = std::thread([this]()
        {
            logger_->info("WebSocket server IOC thread started.");
            ioc_.run();
        });
    }
    do_accept();
}

void WebSocketServer::Stop()
{
    if (acceptor_.is_open())
    {
        acceptor_.close();
        logger_->info("WebSocket server acceptor is closed.");
    }
    ioc_.stop();
    logger_->info("WebSocket server IOC is stopped.");
    if (ioc_thread_.joinable())
    {
        ioc_thread_.join();
        logger_->info("WebSocket server IOC thread joined.");
    }
}

void WebSocketServer::broadcast_message_to_websocket(const std::string& message)
{
    for (auto& [id, session] : sessions_)
    {
        if (auto shared_session = session.lock())
        {
            shared_session->send_message(message);
        }
    }
    logger_->info("Broadcasted to all WebSocket clients: {}", message);
}

void WebSocketServer::register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client)
{
    if (!client)
    {
        logger_->warn("Attempted to register a null backend client.");
        return;
    }
    backend_clients_[client->GetName()] = client;
    logger_->info("Registered backend client: {}", client->GetName());
}

void WebSocketServer::deregister_backend_client(const std::string& client_name)
{
    if (backend_clients_.erase(client_name))
    {
        logger_->info("Deregistered backend client: {}", client_name);
    }
    else
    {
        logger_->warn("Attempted to deregister unknown backend client: {}", client_name);
    }
}

void WebSocketServer::close_session(const std::string& session_id)
{
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
    {
        if (auto session = it->second.lock())
        {
            session->send_message("Session closing...");
            session->close();
            logger_->info("Web Socket Session {} closed.", session_id);
        }
        sessions_.erase(it);
    }
    else
    {
        logger_->warn("Attempted to close unknown session: {}", session_id);
    }
}

void WebSocketServer::register_session(std::shared_ptr<WebSocketSession> session)
{
    sessions_[session->GetSessionID()] = session;
    logger_->info("Web Socket Session {} registered.", session->GetSessionID());
}

void WebSocketServer::do_accept()
{
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
                register_session(session);
                session->run();
            }
            else
            {
                logger_->warn("Web Socket Server connection accept error: {}", ec.message());
            }
            do_accept();
        });
}