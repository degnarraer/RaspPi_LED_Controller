#pragma once

#include <algorithm>
#include <sstream>
#include <memory>
#include <mutex>
#include <iostream>
#include <stdexcept>


template <typename T>
std::string encode_signal_name_and_value(const std::string& signal, const T& value)
{
    json j;
    j["signal"] = signal;
    j["value"] = value;
    return j.dump();
}

template <typename T>
json encode_labels_values_from_2_vectors(const std::vector<std::string>& labels, const std::vector<T>& values)
{
    if (labels.size() != values.size())
    {
        throw std::invalid_argument("Labels and values vectors must have the same size.");
    }

    json j;
    j["labels"] = labels;
    j["values"] = values;
    return j;
}

template <typename T>
inline std::string to_string(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

template <typename T>
inline std::string to_string(const std::vector<T>& vec)
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

template<typename T>
Signal<T>::Signal(const std::string& name)
    : name_(name), webSocketServer_(nullptr), data_(std::make_shared<T>()), encoder_(nullptr), isUsingWebSocket_(false)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
}

template<typename T>
Signal<T>::Signal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder encoder)
    : name_(name), webSocketServer_(webSocketServer), data_(std::make_shared<T>()), encoder_(encoder ? encoder : [](const std::string signal, const T& value) { return to_string(value); }) , isUsingWebSocket_(true)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
}

template<typename T>
void Signal<T>::Setup()
{
    if (webSocketServer_)
    {
        webSocketServer_->register_backend_client(this->shared_from_this());
    }
    else
    {
        logger_->warn("WebSocketServer is not initialized.");
    }
}

template<typename T>
void Signal<T>::SetValue(const T& value, void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!data_)
    {
        logger_->error("Data is not initialized!");
        return;
    }

    if (logger_)
    {
        logger_->debug("SetValue");
    }

    *data_ = value;
    NotifyClients(arg);
    NotifyWebSocket();
}

template<typename T>
T Signal<T>::GetValue() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!data_)
    {
        logger_->error("Data is not initialized!");
        return T();  // Return default-constructed T
    }

    if (logger_)
    {
        logger_->debug("GetValue");
    }

    return *data_;
}

template<typename T>
void Signal<T>::RegisterCallback(Callback cb, void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_)
    {
        logger_->debug("Register Callback");
    }

    auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
        [arg](const typename ISignalValue<T>::CallbackData& data) { return data.arg == arg; });

    if (it != callbacks_.end())
    {
        if (logger_)
        {
            logger_->debug("Existing Callback Updated.");
        }
        it->callback = std::move(cb);
    }
    else
    {
        if (logger_)
        {
            logger_->debug("New Callback Registered.");
        }
        callbacks_.emplace_back(typename ISignalValue<T>::CallbackData{std::move(cb), arg});
    }
}

template<typename T>
void Signal<T>::UnregisterCallbackByArg(void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_)
    {
        logger_->debug("Callback Unregistered.");
    }

    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
        [arg](const typename ISignalValue<T>::CallbackData& data) { return data.arg == arg; }), callbacks_.end());
}

template<typename T>
const std::string& Signal<T>::GetName() const
{
    return name_;
}

template<typename T>
void Signal<T>::on_message_received_from_web_socket(const std::string& message)
{
    if (logger_)
    {
        logger_->info("Received WebSocket message: {}", message);
    }
}

template<typename T>
void Signal<T>::NotifyClients(void* arg)
{
    if (logger_)
    {
        logger_->debug("NotifyClients.");
    }

    if (callbacks_.empty())
    {
        if (logger_)
        {
            logger_->debug("No callbacks registered.");
        }
        return;
    }

    for (const auto& aCallback : callbacks_)
    {
        if (aCallback.callback)
        {
            aCallback.callback(*data_, aCallback.arg);
        }
        else
        {
            if (logger_)
            {
                logger_->warn("Found a null callback.");
            }
        }
    }
}


template<typename T>
void Signal<T>::NotifyWebSocket()
{
    if(!isUsingWebSocket_) return;
    if (!webSocketServer_)
    {
        logger_->error("WebSocketServer is not initialized.");
        return;
    }

    if (!encoder_)
    {
        logger_->error("Encoder is not initialized.");
        return;
    }

    if (logger_)
    {
        logger_->debug("NotifyWebSocket: {}", to_string(*data_));
    }

    std::string jsonMessage = encoder_(name_, *data_);
    webSocketServer_->broadcast_signal_to_websocket(name_, jsonMessage);
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::CreateSignal(const std::string& name)
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
std::shared_ptr<Signal<T>> SignalManager::CreateSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, typename Signal<T>::JsonEncoder encoder)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        return std::static_pointer_cast<Signal<T>>(it->second);
    }

    auto signal = std::make_shared<Signal<T>>(name, webSocketServer, encoder);
    signal->Setup();
    signals_[name] = signal;
    return signal;
}
