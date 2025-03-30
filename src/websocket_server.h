#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <spdlog/spdlog.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#pragma once

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
public:
    explicit WebSocketSession(tcp::socket socket)


        : ws_(std::move(socket))
    {
        session_id_ = boost::uuids::to_string(boost::uuids::random_generator()());
        logger_ = spdlog::get("WebSocketSessionLogger");
        
        if (!logger_)
        {
            logger_ = spdlog::stdout_color_mt("WebSocketSessionLogger");
        }
        
        logger_->info("Created new web socket session: {}", session_id_);
    }

    void run()
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

    // Getter to retrieve the session ID (UUID)
    std::string GetSessionID() const
    {
        return session_id_;
    }

    void send_message(const std::string& message)
    {
        ws_.text(ws_.got_text());  // Ensure we're sending text frames
        ws_.async_write(boost::asio::buffer(message),
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                if (ec)
                {
                    self->logger_->error("Error sending message: {}", ec.message());
                }
            });
    }

private:
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string session_id_; // Session ID (UUID)
    std::shared_ptr<spdlog::logger> logger_;

    void do_read()
    {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred)
            {
                if (ec)
                {
                    self->logger_->warn("Read error: {}", ec.message());
                    return;  // Exit on error
                }
                
                self->on_read(bytes_transferred);
            });
    }

    void on_read(std::size_t)
    {
        auto message = beast::buffers_to_string(buffer_.data());
        logger_->info("Received: {}", message);
        buffer_.consume(buffer_.size());
        
        // Respond with an echo message
        do_write("Echo: " + message);
    }

    void do_write(const std::string& message)
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
};

class IWebSocketServer_BackendClient
{
public:
    virtual ~IWebSocketServer_BackendClient() = default;

    // Get Backend Client Name
    virtual std::string GetName() = 0;
    
    // Callback for receiving messages
    virtual void on_message_received_from_web_socket(const std::string& message) = 0;
};

class WebSocketServer
{
public:
    WebSocketServer(short port)
        : port_(port)
        , ioc_()
        , acceptor_(ioc_, tcp::endpoint(tcp::v4(), port_))
    {
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        
        logger_ = spdlog::get("WebSocketServerLogger");
        
        if (!logger_)
        {
            logger_ = spdlog::stdout_color_mt("WebSocketServerLogger");
        }
    }

    ~WebSocketServer()
    {
        Stop();
    }

    void Run()
    {
        // Ensure acceptor is open before accepting connections
        if (!acceptor_.is_open())
        {
            acceptor_.open(tcp::v4());
            logger_->info("WebSocket server acceptor is opened.");
        }

        // Start the IO thread if it is not already running
        if (!ioc_thread_.joinable())
        {
            ioc_.restart(); // Reset io_context to allow reuse
            ioc_thread_ = std::thread([this]()
            {
                logger_->info("WebSocket server IOC thread started.");
                ioc_.run();
            });
        }
        else
        {
            logger_->warn("WebSocket server IOC thread is already running.");
        }

        do_accept();
    }

    void Stop()
    {
        if (!acceptor_.is_open() && !ioc_thread_.joinable())
        {
            logger_->warn("WebSocket server is already stopped.");
            return;
        }

        if (acceptor_.is_open())
        {
            acceptor_.close();
            logger_->info("WebSocket server acceptor is closed.");
        }

        ioc_.stop();
        logger_->info("WebSocket server IOC is stopped.");

        if (ioc_thread_.joinable())
        {
            ioc_thread_.join();  // Ensure the thread exits cleanly
            logger_->info("WebSocket server IOC thread joined.");
        }
    }

    void broadcast_message_to_websocket(const std::string& message)
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

    // Register a backend client
    void register_backend_client(std::shared_ptr<IWebSocketServer_BackendClient> client)
    {
        if (!client)
        {
            logger_->warn("Attempted to register a null backend client.");
            return;
        }
        
        backend_clients_[client->GetName()] = client;
        logger_->info("Registered backend client: {}", client->GetName());
    }

    // Deregister a backend client
    void deregister_backend_client(const std::string& client_name)
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

private:
    boost::asio::io_context ioc_;
    short port_;
    tcp::acceptor acceptor_;
    std::thread ioc_thread_;  // Thread to run the io_context
    std::unordered_map<std::string, std::weak_ptr<WebSocketSession>> sessions_;
    std::unordered_map<std::string, std::shared_ptr<IWebSocketServer_BackendClient>> backend_clients_;
    std::shared_ptr<spdlog::logger> logger_;
    
    void register_session(std::shared_ptr<WebSocketSession> session)
    {
        sessions_[session->GetSessionID()] = session;
        logger_->info("Session {} registered.", session->GetSessionID());
    }

    void do_accept()
    {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    auto session = std::make_shared<WebSocketSession>(std::move(socket));
                    register_session(session);
                    session->run();
                }
                else
                {
                    logger_->warn("Accept error: {}", ec.message());
                }
                
                do_accept();  // Accept the next connection
            });
    }
};
