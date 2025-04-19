#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include "logger.h"
#include "websocket_server.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Point
{
    float x;
    float y;
};

std::string encode_signal_name_and_value(const std::string& signal, const json& value);
std::string encode_FFT_Bands(const std::string& signal, const std::vector<float>& values);

template <typename T>
json encode_labels_values_from_2_vectors(const std::vector<std::string>& labels, const std::vector<T>& values);

template <typename T>
std::string to_string(const std::vector<T>& vec);

template<typename T>
class Signal : public IWebSocketServer_BackendClient, public std::enable_shared_from_this<Signal<T>>
{
public:
    using Callback = std::function<void(const T&, void*)>;
    using JsonEncoder = std::function<std::string(const std::string&, const T&)>;

    struct CallbackData
    {
        Callback callback;
        void* arg;
    };

    explicit Signal(const std::string& name);
    Signal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder encoder = nullptr);

    void Setup();
    void SetValue(const T& value, void* arg = nullptr);
    T GetValue() const;
    void RegisterCallback(Callback cb, void* arg = nullptr);
    void UnregisterCallbackByArg(void* arg);
    const std::string& GetName() const;
    void on_message_received_from_web_socket(const std::string& message) override;

private:
    const std::string& name_;
    std::shared_ptr<T> data_;
    mutable std::mutex mutex_;
    std::vector<CallbackData> callbacks_;
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::shared_ptr<spdlog::logger> logger_;
    JsonEncoder encoder_;

    void NotifyClients(const T& value, void* arg);
    void NotifyWebSocket(const T& value);
};

class SignalManager
{
public:
    static SignalManager& GetInstance();

    template<typename T>
    std::shared_ptr<Signal<T>> GetSignal(const std::string& name);

    template<typename T>
    std::shared_ptr<Signal<T>> GetSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, typename Signal<T>::JsonEncoder encoder = nullptr);

private:
    SignalManager();
    ~SignalManager() = default;
    SignalManager(const SignalManager&) = delete;
    SignalManager& operator=(const SignalManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<void>> signals_;
    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

#include "signal.tpp"