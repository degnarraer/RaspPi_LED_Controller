#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <memory>

// Templated Signal class
template<typename T>
class Signal {
public:
    using Callback = std::function<void(const T&)>;

    Signal(const std::string& name) : name_(name), data_(std::make_shared<T>()) {}

    void SetValue(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        *data_ = value;
        NotifyClients(value);
    }

    std::shared_ptr<T> GetValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

    void RegisterCallback(Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.insert(std::move(cb));
    }

    void UnregisterCallback(const Callback& cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(cb);
    }

    const std::string& GetName() const { return name_; }

private:
    void NotifyClients(const T& value) {
        for (const auto& cb : callbacks_) {
            cb(value);
        }
    }

    std::string name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::unordered_set<Callback> callbacks_;
};
