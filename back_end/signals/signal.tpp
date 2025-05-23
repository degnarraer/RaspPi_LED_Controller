#pragma once

#include <algorithm>
#include <sstream>
#include <memory>
#include <mutex>
#include <iostream>
#include <stdexcept>


template<typename T>
Signal<T>::Signal( const std::string& name )
                 : name_(name)
                 , webSocketServer_(nullptr)
                 , data_(std::make_shared<T>())
                 , jsonEncoder_(nullptr)
                 , binaryEncoder_(nullptr)
                 , isUsingWebSocket_(false)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    logger_->info("Created Signal\n Name: {}\n Type: Internal ", name_);
}

template<typename T>
Signal<T>::Signal( const std::string& name
                 , std::shared_ptr<WebSocketServer> webSocketServer
                 , JsonEncoder<T> jsonEncoder
                 , MessagePriority priority
                 , bool should_retry )
                 : name_(name)
                 , webSocketServer_(webSocketServer)
                 , data_(std::make_shared<T>())
                 , jsonEncoder_(jsonEncoder)
                 , binaryEncoder_(nullptr)
                 , priority_(priority)
                 , should_retry_(should_retry)
                 , isUsingWebSocket_(true)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    logger_->info("Created Signal\n Name: {}\n Type: json WebSocket", name_);
}

template<typename T>
Signal<T>::Signal( const std::string& name
                 , std::shared_ptr<WebSocketServer> webSocketServer
                 , BinaryEncoder<T> binaryEncoder
                 , MessagePriority priority
                 , bool should_retry )
                 : name_(name)
                 , webSocketServer_(webSocketServer)
                 , data_(std::make_shared<T>())
                 , jsonEncoder_(nullptr)
                 , binaryEncoder_(binaryEncoder)
                 , priority_(priority)
                 , should_retry_(should_retry)
                 , isUsingWebSocket_(true)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    logger_->info("Created Signal\n Name: {}\n Type: binary WebSocket", name_);
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
    logger_->debug("NotifyClients.");

    if (callbacks_.empty())
    {
        logger_->debug("No callbacks registered.");
        return;
    }

    if (!data_)
    {
        logger_->error("{}: Data is not initialized.", name_);
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
            logger_->warn("Found a null callback.");
        }
    }
}


template<typename T>
void Signal<T>::NotifyWebSocket()
{
    if(!isUsingWebSocket_) return;
    if (!data_)
    {
        logger_->error("{}: Data is not initialized.", name_);
        return;
    }
    logger_->debug("NotifyWebSocket: {}", to_string(*data_));
    
    if (!webSocketServer_)
    {
        logger_->error("{}: WebSocketServer is not initialized.", name_);
        return;
    }
    
    if (!jsonEncoder_ && !binaryEncoder_)
    {
        logger_->error("{}: Encoder is not initialized.", name_);
        return;
    }

    if(jsonEncoder_)
    {
        std::string jsonMessage = jsonEncoder_(name_, *data_);
        webSocketServer_->broadcast_signal_to_websocket(name_, WebSocketMessage(jsonMessage, priority_, should_retry_));
    }
    else if(binaryEncoder_)
    {
        const std::vector<uint8_t> binaryMessage = binaryEncoder_(name_, *data_);
        webSocketServer_->broadcast_signal_to_websocket(name_, WebSocketMessage(binaryMessage, priority_, should_retry_));
    }
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
std::shared_ptr<Signal<T>> SignalManager::CreateSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder<T> encoder)
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

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::CreateSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, BinaryEncoder<T> encoder)
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
