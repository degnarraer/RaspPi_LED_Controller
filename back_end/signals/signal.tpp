#pragma once

#include <algorithm>
#include <sstream>
#include <memory>
#include <mutex>
#include <iostream>
#include <stdexcept>


template<typename T>
T SignalValue<T>::GetValue() const
{
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (!data_)
    {
        this->logger_->error("Data is not initialized!");
        return T();
    }

    if (this->logger_)
    {
        this->logger_->debug("GetValue");
    }
    return *data_;
}

template<typename T>
void SignalValue<T>::RegisterSignalValueCallback(SignalValueCallback cb, void* arg)
{
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->logger_)
    {
        this->logger_->debug("Register Callback");
    }

    auto it = std::find_if(this->callbacks_.begin(), this->callbacks_.end(),
        [arg](const typename SignalValue<T>::SignalValueCallbackData& data) { return data.arg == arg; });

    if (it != this->callbacks_.end())
    {
        if (this->logger_)
        {
            this->logger_->debug("Existing Callback Updated.");
        }
        it->callback = std::move(cb);
    }
    else
    {
        if (this->logger_)
        {
            this->logger_->debug("New Callback Registered.");
        }
        this->callbacks_.emplace_back(typename SignalValue<T>::SignalValueCallbackData{std::move(cb), arg});
    }
}

template<typename T>
void SignalValue<T>::UnregisterSignalValueCallbackByArg(void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (this->logger_)
    {
        this->logger_->debug("Callback Unregistered.");
    }

    this->callbacks_.erase(std::remove_if(this->callbacks_.begin(), this->callbacks_.end(),
        [arg](const typename SignalValue<T>::SignalValueCallbackData& data) { return data.arg == arg; }), this->callbacks_.end());
}

template<typename T>
void SignalValue<T>::SetValue(const T& value, void* arg)
{
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (!data_)
    {
        this->logger_->error("Data is not initialized!");
        return;
    }

    if (this->logger_)
    {
        this->logger_->debug("SetValue");
    }

    *data_ = value;
}


template<typename T>
Signal<T>::Signal( const std::string& name )
                 : SignalValue<T>(name)
                 , webSocketServer_(nullptr)
                 , jsonEncoder_(nullptr)
                 , binaryEncoder_(nullptr)
                 , isUsingWebSocket_(false)
{
    this->logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    this->logger_->info("Created Signal\n Name: {}\n Type: Internal ", this->name_);
}

template<typename T>
Signal<T>::Signal( const std::string& name
                 , std::shared_ptr<WebSocketServer> webSocketServer
                 , JsonEncoder<T> jsonEncoder
                 , MessagePriority priority
                 , bool should_retry )
                 : SignalValue<T>(name)
                 , webSocketServer_(webSocketServer)
                 , jsonEncoder_(jsonEncoder)
                 , binaryEncoder_(nullptr)
                 , priority_(priority)
                 , should_retry_(should_retry)
                 , isUsingWebSocket_(true)
{
    this->logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    this->logger_->info("Created Signal\n Name: {}\n Type: json WebSocket", this->name_);
}

template<typename T>
Signal<T>::Signal( const std::string& name
                 , std::shared_ptr<WebSocketServer> webSocketServer
                 , BinaryEncoder<T> binaryEncoder
                 , MessagePriority priority
                 , bool should_retry )
                 : SignalValue<T>(name)
                 , webSocketServer_(webSocketServer)
                 , jsonEncoder_(nullptr)
                 , binaryEncoder_(binaryEncoder)
                 , priority_(priority)
                 , should_retry_(should_retry)
                 , isUsingWebSocket_(true)
{
    this->logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
    this->logger_->info("Created Signal\n Name: {}\n Type: binary WebSocket", this->name_);
}

template<typename T>
void Signal<T>::Setup()
{
    if (webSocketServer_)
    {
        //webSocketServer_->register_backend_client(this->shared_from_this());
    }
    else
    {
        this->logger_->warn("WebSocketServer is not initialized.");
    }
}

template<typename T>
void Signal<T>::SetValue(const T& value, void* arg)
{
    if (this->logger_)
    {
        this->logger_->debug("SetValue");
    }

    SignalValue<T>::SetValue(value, arg);
    NotifyClients(arg);
    NotifyWebSocket();
}

template<typename T>
void Signal<T>::NotifyClients(void* arg)
{
    this->logger_->debug("NotifyClients.");

    if (this->callbacks_.empty())
    {
        this->logger_->debug("No callbacks registered.");
        return;
    }

    if (!this->data_)
    {
        this->logger_->error("{}: Data is not initialized.", this->name_);
        return;
    }

    for (const auto& aCallback : this->callbacks_)
    {
        if (aCallback.callback)
        {
            aCallback.callback(*this->data_, aCallback.arg);
        }
        else
        {
            this->logger_->warn("Found a null callback.");
        }
    }
}

template<typename T>
void Signal<T>::NotifyWebSocket()
{
    if(!isUsingWebSocket_) return;
    if (!this->data_)
    {
        this->logger_->error("{}: Data is not initialized.", this->name_);
        return;
    }
    this->logger_->debug("NotifyWebSocket: {}", to_string(*this->data_));
    
    if (!webSocketServer_)
    {
        this->logger_->error("{}: WebSocketServer is not initialized.", this->name_);
        return;
    }
    
    if (!jsonEncoder_ && !binaryEncoder_)
    {
        this->logger_->error("{}: Encoder is not initialized.", this->name_);
        return;
    }

    if(jsonEncoder_)
    {
        std::string jsonMessage = jsonEncoder_(this->name_, *this->data_);
        webSocketServer_->broadcast_signal_to_websocket(this->name_, WebSocketMessage(jsonMessage, priority_, should_retry_));
    }
    else if(binaryEncoder_)
    {
        const std::vector<uint8_t> binaryMessage = binaryEncoder_(this->name_, *this->data_);
        webSocketServer_->broadcast_signal_to_websocket(this->name_, WebSocketMessage(binaryMessage, priority_, should_retry_));
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
