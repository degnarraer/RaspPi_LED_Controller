#pragma once

#include <algorithm>
#include <sstream>

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

template<typename T>
Signal<T>::Signal(const std::string& name)
    : name_(name), webSocketServer_(nullptr), data_(std::make_shared<T>()), encoder_(nullptr)
{
    logger_ = InitializeLogger(name + " Signal Logger", spdlog::level::info);
}

template<typename T>
Signal<T>::Signal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder encoder)
    : name_(name), webSocketServer_(webSocketServer), data_(std::make_shared<T>())
    , encoder_(encoder ? encoder : [](const std::string signal, const T& value) { return to_string(value); })
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
}

template<typename T>
void Signal<T>::SetValue(const T& value, void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logger_->debug("SetValue");
    *data_ = value;
    NotifyClients(value, arg);
    NotifyWebSocket(value);
}

template<typename T>
T Signal<T>::GetValue() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    logger_->debug("GetValue");
    return *data_;
}

template<typename T>
void Signal<T>::RegisterCallback(Callback cb, void* arg)
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

template<typename T>
void Signal<T>::UnregisterCallbackByArg(void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logger_->debug("Callback Unregistered.");
    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
        [arg](const CallbackData& data) { return data.arg == arg; }), callbacks_.end());
}

template<typename T>
const std::string& Signal<T>::GetName() const
{
    return name_;
}

template<typename T>
void Signal<T>::on_message_received_from_web_socket(const std::string& message)
{
    logger_->info("Received WebSocket message: {}", message);
}

template<typename T>
void Signal<T>::NotifyClients(const T& value, void* arg)
{
    logger_->debug("NotifyClients.");
    for (const auto& data : callbacks_)
    {
        data.callback(value, data.arg);
    }
}

template<typename T>
void Signal<T>::NotifyWebSocket(const T& value)
{
    if (webSocketServer_ && encoder_)
    {
        logger_->debug("NotifyWebSocket: {}", to_string(value));
        std::string jsonMessage = encoder_(name_, value);
        webSocketServer_->broadcast_signal_to_websocket(name_, jsonMessage);
    }
}

template<typename T>
std::shared_ptr<Signal<T>> SignalManager::GetSignal(const std::string& name)
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
std::shared_ptr<Signal<T>> SignalManager::GetSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, typename Signal<T>::JsonEncoder encoder)
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
