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


/*
template<typename T>
bool SignalValue<T>::setValueFromString(const std::string& value_str)
{
    try
    {
        *data_ = from_string(value_str);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Parsing failed: " << e.what() << "\n";
        return false;
    }
}
*/

template<typename T>
bool SignalValue<T>::setValueFromJSON(const json& j)
{
    try
    {
        this->setValue(j.get<T>());
        return true;
    }
    catch(const std::exception& e)
    {
        this->logger_->error("JSON parsing failed: {}", e.what());
        return false;
    }
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
                 , std::weak_ptr<WebSocketServer> webSocketServer
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
                 , std::weak_ptr<WebSocketServer> webSocketServer
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
    auto server = webSocketServer_.lock();
    if (server)
    {
        server->register_notification_client(this->getName(), this);
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
    if (!isUsingWebSocket_) return;

    if (!this->data_)
    {
        this->logger_->error("{}: Data is not initialized.", this->name_);
        return;
    }

    auto server = webSocketServer_.lock();
    if (!server)
    {
        this->logger_->error("{}: WebSocketServer has expired or not set for {}", this->name_);
        return;
    }

    if (!jsonEncoder_ && !binaryEncoder_)
    {
        this->logger_->error("{}: No encoder provided.", this->name_);
        return;
    }

    std::shared_ptr<T> dataCopy;
    {
        std::lock_guard<std::mutex> lock(this->dataMutex_);
        dataCopy = this->data_;
    }

    if (!dataCopy) return;

    // Protect to_string from crashing
    try
    {
        this->logger_->debug("NotifyWebSocket: {}", to_string(*dataCopy));
    }
    catch (const std::exception& e)
    {
        this->logger_->warn("{}: Exception in to_string: {}", this->name_, e.what());
    }
    catch (...)
    {
        this->logger_->warn("{}: Unknown exception in to_string", this->name_);
    }

    // JSON Encoder
    if (jsonEncoder_)
    {
        try
        {
            auto msg = jsonEncoder_(this->name_, *dataCopy);
            auto wsMsg = std::make_shared<WebSocketMessage>(msg, priority_, should_retry_);
            server->broadcast_signal_to_websocket(this->name_, std::move(wsMsg));
        }
        catch (const std::exception& e)
        {
            this->logger_->error("{}: Exception in jsonEncoder: {}", this->name_, e.what());
        }
        catch (...)
        {
            this->logger_->error("{}: Unknown exception in jsonEncoder", this->name_);
        }
    }

    // Binary Encoder
    if (binaryEncoder_)
    {
        try
        {
            auto msg = binaryEncoder_(this->name_, *dataCopy);
            if (!msg.empty())
            {
                auto wsMsg = std::make_shared<WebSocketMessage>(msg, priority_, should_retry_);
                server->broadcast_signal_to_websocket(this->name_, std::move(wsMsg));
            }
            else
            {
                this->logger_->warn("{}: Binary encoder returned empty message, not sending.", this->name_);
            }
        }
        catch (const std::exception& e)
        {
            this->logger_->error("{}: Exception in binaryEncoder: {}", this->name_, e.what());
        }
        catch (...)
        {
            this->logger_->error("{}: Unknown exception in binaryEncoder", this->name_);
        }
    }
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name)
{
    std::lock_guard<std::mutex> lock(signal_mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        auto existing = std::dynamic_pointer_cast<Signal<T>>(it->second);
        if (!existing)
        {
            throw std::runtime_error("Type mismatch for signal: " + name);
        }
        return existing;
    }

    auto signal = std::make_shared<Signal<T>>(name);
    signals_[name] = signal;
    return signal;
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name,
                                                       std::shared_ptr<WebSocketServer> webSocketServer,
                                                       JsonEncoder<T> encoder)
{
    std::lock_guard<std::mutex> lock(signal_mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        auto existing = std::dynamic_pointer_cast<Signal<T>>(it->second);
        if (!existing)
        {
            throw std::runtime_error("Type mismatch for signal: " + name);
        }
        return existing;
    }

    auto signal = std::make_shared<Signal<T>>(name, webSocketServer, encoder);
    signal->setup();
    signals_[name] = signal;
    return signal;
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::createSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, BinaryEncoder<T> encoder)
{
    std::lock_guard<std::mutex> lock(signal_mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        auto existing = std::dynamic_pointer_cast<Signal<T>>(it->second);
        if (!existing)
        {
            throw std::runtime_error("Type mismatch for signal: " + name);
        }
        return existing;
    }

    auto signal = std::make_shared<Signal<T>>(name, webSocketServer, encoder);
    signal->setup();
    signals_[name] = signal;
    return signal;
}

