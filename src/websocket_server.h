#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <spdlog/spdlog.h>

#pragma once

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket) : ws_(std::move(socket))
    {
        logger = spdlog::get("WebSocketSessionLogger");
        if (!logger)
        {
            logger = spdlog::stdout_color_mt("WebSocketSessionLogger");
        }
    }

    void run() {
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec)
            {
                if (!ec) self->do_read();
                else {
                    self->logger->warn("Accept error: {}", ec.message());
                }
            });
    }

    void send_message(const std::string& message)
    {
        ws_.text(ws_.got_text());  // Ensure we're sending text frames
        ws_.async_write(boost::asio::buffer(message),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec)
                {
                    self->logger->error("Error sending message: {}", ec.message());
                }
            });
    }

private:
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::shared_ptr<spdlog::logger> logger;

    void do_read() {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    self->logger->warn("Read error: {}", ec.message());
                    return;  // Exit on error
                }
                self->on_read(bytes_transferred);
            });
    }

    void on_read(std::size_t)
    {
        auto message = beast::buffers_to_string(buffer_.data());
        logger->info("Received: {}", message);
        buffer_.consume(buffer_.size());
        // Respond with an echo message
        do_write("Echo: " + message);
    }

    void do_write(const std::string& message) {
        logger->info("Send: {}", message);
        ws_.text(ws_.got_text());
        ws_.async_write(boost::asio::buffer(message),
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                if (!ec) self->do_read();
                else {
                    self->logger->warn("Write error: {}", ec.message());
                }
            });
    }
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
        
        logger = spdlog::get("WebSocketServerLogger");
        if (!logger)
        {
            logger = spdlog::stdout_color_mt("WebSocketServerLogger");
        }
    }

    ~WebSocketServer()
    {
        Stop();
    }

    void Run()
    {
        Stop();

        // Open the acceptor to start accepting connections
        acceptor_.open(tcp::v4());
        
        // Start the WebSocket server
        ioc_thread_ = std::thread([this]() { ioc_.run(); });

        do_accept();
        logger->info("WebSocket server is running on ws://localhost:8080");
    }

    void Stop()
    {
        acceptor_.close();
        ioc_.stop();
        if (ioc_thread_.joinable())
        {
            ioc_thread_.join();  // Join the thread to ensure it exits cleanly
        }
        logger->info("WebSocket server stopped.");
    }

private:
    boost::asio::io_context ioc_;
    short port_;
    tcp::acceptor acceptor_;
    std::thread ioc_thread_;  // Thread to run the io_context
    std::shared_ptr<spdlog::logger> logger;

    void do_accept()
    {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket)
            {
                if (!ec) std::make_shared<WebSocketSession>(std::move(socket))->run();
                else
                {
                    logger->warn("Accept error: {}", ec.message());
                }
                do_accept();  // Accept the next connection
            });
    }
};
