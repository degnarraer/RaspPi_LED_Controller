#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include "logger.h"
#include "websocket_server.h"

// Templated Signal class
template<typename T>
class Signal: public IWebSocketServer_BackendClient {
public:
    // Struct to store callback and its associated argument
    using Callback = std::function<void(const T&, void*)>;
    struct CallbackData {
        Callback callback;
        void* arg;
    };

    Signal( const std::string& name)
          : name_(name)
          , data_(std::make_shared<T>())
          {
            logger_ = InitializeLogger(name + " Signal Logger" , spdlog::level::info);
          }
    Signal( const std::string& name, std::shared_ptr<WebSocketServer> server)
          : name_(name)
          , data_(std::make_shared<T>())
          , server_(server)
          {
            logger_ = InitializeLogger(name + " Signal Logger" , spdlog::level::info);
            server->register_backend_client(this);
          }

    void SetValue(const T& value, void* arg = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("SetValue");
        *data_ = value;
        NotifyClients(value, arg);
    }

    std::shared_ptr<T> GetValue() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("GetValue");
        return data_;
    }

    void RegisterCallback(Callback cb, void* arg = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("Register Callback");
        
        // Check if the arg is already registered
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { return data.arg == arg; });

        if (it != callbacks_.end())
        {
            logger_->debug("Existing Callback Updated.");
            it->callback = std::move(cb);
        }
        else
        {
            // If not found, add a new callback
            logger_->debug("New Callback Registered.");
            callbacks_.push_back({std::move(cb), arg});
        }
    }

    void UnregisterCallbackByArg(void* arg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_->debug("Callback Unregistered.");
        // Remove the callback if the argument matches
        auto it = std::remove_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data)
            { 
                return data.arg == arg;  // Compare only the argument
            });
        callbacks_.erase(it, callbacks_.end());
    }

    const std::string& GetName() const { return name_; }


//IWebSocketServer_BackendClient Interface
    // Get Client Name
    std::string GetName()
    {
        return name_;
    }

    // Callback for receiving messages from web socket
    void on_message_received_from_web_socket(const std::string& message)
    {

    }

private:
    void NotifyClients(const T& value, void* arg)
    {
        logger_->debug("NotifyClients.");
        for (const auto& data : callbacks_)
        {
            data.callback(value, data.arg);  // Pass the argument stored in the struct
        }
    }

    void NotifyWebSocket(const T& value)
    {
        if(server_)
        {
            server_->broadcast_message_to_websocket(std::to_string(value));
        }
    }

    std::string name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::vector<CallbackData> callbacks_;  // Store callback and arg together
    std::shared_ptr<WebSocketServer> server_;
    std::shared_ptr<spdlog::logger> logger_;
};

class SignalManager
{
    public:
        static SignalManager& GetInstance()
        {
            static SignalManager instance;
            return instance;
        }
    
        // Template function to get a signal by name
        template<typename T>
        std::shared_ptr<Signal<T>> GetSignal(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = signals_.find(name);
            if (it != signals_.end())
            {
                // If signal exists, return it
                return std::static_pointer_cast<Signal<T>>(it->second);
            }
            else
            {
                // If signal doesn't exist, create a new one and return
                auto signal = std::make_shared<Signal<T>>(name);
                signals_[name] = signal;
                return signal;
            }
        }
    
    private:
        SignalManager() = default;
        ~SignalManager() = default;
    
        SignalManager(const SignalManager&) = delete;
        SignalManager& operator=(const SignalManager&) = delete;
    
        std::shared_ptr<spdlog::logger> logger_;
        std::unordered_map<std::string, std::shared_ptr<void>> signals_;
        std::mutex mutex_;
};
