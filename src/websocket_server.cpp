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
            }
        });
}

void WebSocketSession::close()
{
    logger_->debug("Session: {} Close.", GetSessionID());
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    if (ec)
    {
        logger_->warn("Error closing session: {}", ec.message());
    }
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

                // If queue is full, drop the oldest message
                if (self->outgoing_messages_.size() >= MAX_QUEUE_SIZE)
                {
                    self->outgoing_messages_.pop_front();
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
    send_message("Echo: " + message);
}

void WebSocketSession::do_write()
{
    logger_->debug("Session: {} Do Write.", GetSessionID());

    // Check if the WebSocket is open, if not, attempt to reconnect
    if (!ws_.is_open())
    {
        logger_->warn("WebSocket not open, retrying connection...");
        auto retry_attempts = 5;  // Number of retry attempts for WebSocket connection
        auto retry_delay = std::chrono::seconds(2);  // Delay between retries (e.g., 2 seconds)

        // Lambda to handle retry logic for reconnecting and then writing
        std::function<void()> retry_connection;

        retry_connection = [self = shared_from_this(), retry_attempts, retry_delay, &retry_connection]() mutable
        {
            if (retry_attempts > 0)
            {
                self->logger_->debug("Retry attempt {} of {}", 6 - retry_attempts, 5);
                std::this_thread::sleep_for(retry_delay);  // Delay before retrying connection
                retry_attempts--;  // Decrease retry attempts
                
                // Try reconnecting by calling async_accept again or other reconnect strategy
                self->ws_.async_accept(
                    [self, &retry_connection](beast::error_code ec)
                    {
                        if (!ec)
                        {
                            self->logger_->info("WebSocket successfully reconnected.");
                            self->do_write();  // Once reconnected, attempt to write
                        }
                        else
                        {
                            self->logger_->warn("Reconnection attempt failed: {}", ec.message());
                            // Retry connection again in case of failure
                            retry_connection(); // Recursive retry
                        }
                    });
            }
            else
            {
                self->logger_->error("Failed to reconnect to WebSocket after multiple attempts.");
                self->writing_ = false;  // Stop further write attempts after all retries fail
            }
        };

        // Start retry logic for connection
        retry_connection(); // Initiate the first retry attempt
        return;  // Exit do_write and retry connection before attempting to send data
    }

    // If WebSocket is open, continue with the normal writing process
    std::string message;
    {   // Lock only while accessing the queue
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (outgoing_messages_.empty())
        {
            writing_ = false;
            logger_->debug("Message queue empty. Writing flag reset.");
            return;
        }

        message = std::move(outgoing_messages_.front());
        outgoing_messages_.pop_front();  // Remove the front message from the queue
    }

    // Start writing the message asynchronously
    ws_.text(ws_.got_text());  // Ensure WebSocket text mode is set
    ws_.async_write(asio::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t)
        {
            if (ec)
            {
                self->logger_->warn("Write error: {}", ec.message());
                self->writing_ = false;
            }
            else
            {
                self->logger_->debug("Message sent successfully. Continuing to write if more messages exist.");
                self->do_write();  // Continue writing if more messages are available
            }
        });
}


WebSocketServer::WebSocketServer(short port)
    : ioc_()
    , port_(port)
    , acceptor_(ioc_, tcp::endpoint(tcp::v4(), port_))
{
    logger_ = InitializeLogger("Web Socket Server", spdlog::level::info);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    logger_->debug("Instantiated WebSocketServer.");
}

WebSocketServer::~WebSocketServer()
{
    logger_->debug("Destroying WebSocketServer.");
    Stop();
}

void WebSocketServer::Run()
{
    logger_->debug("Run.");
    if (!acceptor_.is_open())
    {
        beast::error_code ec;
        acceptor_.open(tcp::v4(), ec);
        if (ec)
        {
            logger_->error("Error opening acceptor: {}", ec.message());
            return;
        }
    }

    if (!ioc_thread_.joinable())
    {
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
                logger_->error("Connection accept error: {}.", ec.message());
            }
            do_accept();
        });
}