#pragma once

#include <algorithm>
#include <sstream>
#include <memory>
#include <mutex>
#include <iostream>
#include <stdexcept>


template<typename T>
T SignalValue<T>::getValue() const
{
    std::lock_guard<std::mutex> lock(dataMutex_);
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
bool SignalValue<T>::setValue(const T& value, void* arg)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (!data_)
    {
        this->logger_->error("Data is not initialized!");
        return false;
    }

    if (*data_ != value)
    {
        *data_ = value;
        this->logger_->debug("SetValue - value changed");
        return true;
    }
    this->logger_->debug("SetValue - value unchanged");
    return false;
}

template<typename T>
void SignalValue<T>::registerSignalValueCallback(SignalValueCallback cb, void* arg)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
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
void SignalValue<T>::unregisterSignalValueCallbackByArg(void* arg)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (this->logger_)
    {
        this->logger_->debug("Callback Unregistered.");
    }

    this->callbacks_.erase(std::remove_if(this->callbacks_.begin(), this->callbacks_.end(),
        [arg](const typename SignalValue<T>::SignalValueCallbackData& data) { return data.arg == arg; }), this->callbacks_.end());
}

template<typename T>
Signal<T>::Signal( const std::string& name )
                 : SignalValue<T>(name)
                 , webSocketServer_(nullptr)
                 , jsonEncoder_(nullptr)
                 , binaryEncoder_(nullptr)
                 , isUsingWebSocket_(false)
{
    this->logger_ = initializeLogger(name + " Signal Logger", spdlog::level::info);
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
    this->logger_ = initializeLogger(this->name_ + " Signal Logger", spdlog::level::info);
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
    this->logger_ = initializeLogger(this->name_ + " Signal Logger", spdlog::level::info);
    this->logger_->info("Created Signal\n Name: {}\n Type: binary WebSocket", this->name_);
}

template<typename T>
void Signal<T>::setup()
{
    if (webSocketServer_)
    {
        webSocketServer_->register_notification_client(this->getName(), this);
    }
    else
    {
        this->logger_->warn("WebSocketServer is not initialized.");
    }
}

template<typename T>
bool Signal<T>::setValue(const T& value, void* arg)
{
    bool valueChanged = SignalValue<T>::setValue(value, arg);
    if(valueChanged)
    {
        notifyClients(arg);
        notifyWebSocket();
    }
    return valueChanged;
}

template<typename T>
void Signal<T>::notifyClients(void* arg)
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
void Signal<T>::notifyWebSocket()
{
    if(!isUsingWebSocket_) return;
    if (!this->data_)
    {
        this->logger_->error("{}: Data is not initialized.", this->name_);
        return;
    }    
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

    this->logger_->debug("NotifyWebSocket: {}", to_string(*this->data_));
    if(jsonEncoder_)
    {
        std::string jsonMessage = jsonEncoder_(this->name_, *this->data_);
        auto webSocketMessage = std::make_shared<WebSocketMessage>(WebSocketMessage(jsonMessage, priority_, should_retry_));
        webSocketServer_->broadcast_signal_to_websocket(this->name_, std::move(webSocketMessage));
    }
    if(binaryEncoder_)
    {
        const std::vector<uint8_t> binaryData = binaryEncoder_(this->name_, *this->data_);
        if(!binaryData.empty())
        {
            auto webSocketMessage = std::make_shared<WebSocketMessage>(WebSocketMessage(binaryData, priority_, should_retry_));
            webSocketServer_->broadcast_signal_to_websocket(this->name_, std::move(webSocketMessage));
        }
        else
        {
            this->logger_->warn("Binary message is empty, not sending.");
            return;
        }
    }
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name)
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
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder<T> encoder)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        return std::static_pointer_cast<Signal<T>>(it->second);
    }

    auto signal = std::make_shared<Signal<T>>(name, webSocketServer, encoder);
    signal->setup();
    signals_[name] = signal;
    return signal;
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, BinaryEncoder<T> encoder)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        return std::static_pointer_cast<Signal<T>>(it->second);
    }

    auto signal = std::make_shared<Signal<T>>(name, webSocketServer, encoder);
    signal->setup();
    signals_[name] = signal;
    return signal;
}
