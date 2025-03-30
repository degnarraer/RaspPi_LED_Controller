#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include "websocket_server.h"

#pragma once

// Templated Signal class
template<typename T>
class Signal {
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
          {}
    Signal( const std::string& name, std::shared_ptr<WebSocketSession> session)
          : name_(name)
          , data_(std::make_shared<T>())
          , session_(session)
          {}

    void SetValue(const T& value, void* arg = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Signal Logger")->debug("SetValue");
        *data_ = value;
        NotifyClients(value, arg);
    }

    std::shared_ptr<T> GetValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Signal Logger")->debug("GetValue");
        return data_;
    }

    void RegisterCallback(Callback cb, void* arg = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Signal Logger")->debug("Register Callback");
        
        // Check if the arg is already registered
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { return data.arg == arg; });

        if (it != callbacks_.end()) {
            spdlog::get("Signal Logger")->debug("Existing Callback Updated.");
            it->callback = std::move(cb);
        } else {
            // If not found, add a new callback
            spdlog::get("Signal Logger")->debug("New Callback Registered.");
            callbacks_.push_back({std::move(cb), arg});
        }
    }

    void UnregisterCallbackByArg(void* arg) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Signal Logger")->debug("Callback Unregistered.");
        // Remove the callback if the argument matches
        auto it = std::remove_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { 
                return data.arg == arg;  // Compare only the argument
            });
        callbacks_.erase(it, callbacks_.end());
    }

    const std::string& GetName() const { return name_; }

private:
    void NotifyClients(const T& value, void* arg) {
        spdlog::get("Signal Logger")->debug("NotifyClients.");
        for (const auto& data : callbacks_) {
            data.callback(value, data.arg);  // Pass the argument stored in the struct
        }
    }

    std::string name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::vector<CallbackData> callbacks_;  // Store callback and arg together
    std::shared_ptr<WebSocketSession> session_;
};

class SignalManager {
    public:
        static SignalManager& GetInstance() {
            static SignalManager instance;
            return instance;
        }
    
        // Template function to get a signal by name
        template<typename T>
        std::shared_ptr<Signal<T>> GetSignal(const std::string& name) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = signals_.find(name);
            if (it != signals_.end()) {
                // If signal exists, return it
                return std::static_pointer_cast<Signal<T>>(it->second);
            } else {
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
    
        std::unordered_map<std::string, std::shared_ptr<void>> signals_;
        std::mutex mutex_;
};
