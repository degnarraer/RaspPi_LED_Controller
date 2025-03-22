#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

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

    Signal(const std::string& name) : name_(name), data_(std::make_shared<T>()) {}

    void SetValue(const T& value, void* arg = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Speed Log Logger")->debug("SetValue");
        *data_ = value;
        NotifyClients(value, arg);
    }

    std::shared_ptr<T> GetValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Speed Log Logger")->debug("GetValue");
        return data_;
    }

    void RegisterCallback(Callback cb, void* arg = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Speed Log Logger")->debug("Register Callback");
        
        // Check if the arg is already registered
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
            [arg](const CallbackData& data) { return data.arg == arg; });

        if (it != callbacks_.end()) {
            spdlog::get("Speed Log Logger")->debug("Existing Callback Updated.");
            it->callback = std::move(cb);
        } else {
            // If not found, add a new callback
            spdlog::get("Speed Log Logger")->debug("New Callback Registered.");
            callbacks_.push_back({std::move(cb), arg});
        }
    }

    void UnregisterCallbackByArg(void* arg) {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::get("Speed Log Logger")->debug("Callback Unregistered.");
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
        spdlog::get("Speed Log Logger")->debug("NotifyClients.");
        for (const auto& data : callbacks_) {
            data.callback(value, data.arg);  // Pass the argument stored in the struct
        }
    }

    std::string name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::vector<CallbackData> callbacks_;  // Store callback and arg together
};
