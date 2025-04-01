#pragma once
#include <string>
#include <sstream>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <unordered_map>
#include "logger.h"
#include "websocket_server.h"

template <typename T>
std::string to_string(const std::vector<T>& vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (i > 0) oss << ", ";
        oss << vec[i];
    }
    oss << "]";
    return oss.str();
}


// Templated Signal class
template<typename T>
class Signal : public IWebSocketServer_BackendClient
             , public std::enable_shared_from_this<Signal<T>>
{
public:
    using Callback = std::function<void(const T&, void*)>;
    struct CallbackData
    {
        Callback callback;
        void* arg;
    };

    explicit Signal(const std::string& name)
        : name_(name)
        , webSocketServer_(nullptr)
        , data_(std::make_shared<T>())
    {
        logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    }

    explicit Signal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer)
        : name_(name)
        , webSocketServer_(webSocketServer)
        , data_(std::make_shared<T>())
    {
        logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    }

    void Setup()
    {
        if (webSocketServer_)
        {
            webSocketServer_->register_backend_client(this->shared_from_this());
        }
    }

    void SetValue(const T& value, void* arg = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("SetValue");
        *data_ = value;
        NotifyClients(value, arg);
        NotifyWebSocket(value);
    }

    T GetValue() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("GetValue");
        return *data_;
    }

    void RegisterCallback(Callback cb, void* arg = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("Register Callback");
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { return data.arg == arg; });

        if (it != callbacks_.end())
        {
            logger_->debug("Existing Callback Updated.");
            it->callback = std::move(cb);
        }
        else
        {
            logger_->debug("New Callback Registered.");
            callbacks_.emplace_back(CallbackData{std::move(cb), arg});
        }
    }

    void UnregisterCallbackByArg(void* arg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("Callback Unregistered.");
        callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { return data.arg == arg; }), callbacks_.end());
    }

    const std::string& GetName() const { return name_; }

    // IWebSocketServer_BackendClient Interface
    std::string GetName() override
    {
        return name_;
    }

    void on_message_received_from_web_socket(const std::string& message) override
    {
        logger_->info("Received WebSocket message: {}", message);
    }

private:
    std::string name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::vector<CallbackData> callbacks_;
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::shared_ptr<spdlog::logger> logger_;
    
    void NotifyClients(const T& value, void* arg)
    {
        logger_->debug("NotifyClients.");
        for (const auto& data : callbacks_)
        {
            data.callback(value, data.arg);
        }
    }

    void NotifyWebSocket(const T& value)
    {
        if (webSocketServer_)
        {
            webSocketServer_->broadcast_message_to_websocket(to_string(value));
        }
    }
};

class SignalManager
{
public:
    static SignalManager& GetInstance()
    {
        static SignalManager instance;
        return instance;
    }

    template<typename T>
    std::shared_ptr<Signal<T>> GetSignal(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signals_.find(name);
        if (it != signals_.end())
        {
            return std::static_pointer_cast<Signal<T>>(it->second);
        }
        auto signal = std::make_shared<Signal<T>>(name);
        signals_[name] = signal;
        return signal;
    }

    template<typename T>
    std::shared_ptr<Signal<T>> GetSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signals_.find(name);
        if (it != signals_.end())
        {
            return std::static_pointer_cast<Signal<T>>(it->second);
        }
        auto signal = std::make_shared<Signal<T>>(name, webSocketServer);
        signal->Setup();
        signals_[name] = signal;
        return signal;
    }

private:
    SignalManager() { logger_ = InitializeLogger("Signal Manager", spdlog::level::info); }
    ~SignalManager() = default;
    SignalManager(const SignalManager&) = delete;
    SignalManager& operator=(const SignalManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<void>> signals_;
    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};