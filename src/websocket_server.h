#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <thread>

using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket) : ws_(std::move(socket)) {}

    void run() {
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec) {
                if (!ec) self->do_read();
            });
    }

    void send_message(const std::string& message) {
        ws_.text(ws_.got_text());  // Ensure we're sending text frames
        ws_.async_write(boost::asio::buffer(message),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "Error sending message: " << ec.message() << std::endl;
                }
            });
    }

private:
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    void do_read() {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    self->on_read(bytes_transferred);
                }
            });
    }

    void on_read(std::size_t) {
        auto message = beast::buffers_to_string(buffer_.data());
        std::cout << "Received: " << message << std::endl;
        buffer_.consume(buffer_.size());
        // Respond with an echo message
        do_write("Echo: " + message);
    }

    void do_write(const std::string& message) {
        ws_.text(ws_.got_text());
        ws_.async_write(boost::asio::buffer(message),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) self->do_read();
            });
    }
};

class WebSocketServer {
public:
    WebSocketServer(boost::asio::io_context& ioc, short port)
        : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
        // Start the WebSocket server in its own thread
        server_thread_ = std::thread([this]() { do_accept(); });
    }

    ~WebSocketServer() {
        if (server_thread_.joinable()) {
            server_thread_.join();  // Join the server thread to ensure it completes before exiting
        }
    }

private:
    tcp::acceptor acceptor_;
    std::thread server_thread_;  // Thread to run the WebSocket server

    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) std::make_shared<WebSocketSession>(std::move(socket))->run();
                do_accept();
            });
    }
};
