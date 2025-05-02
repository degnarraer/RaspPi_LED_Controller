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
#include <type_traits>

using json = nlohmann::json;

struct Point
{
    float x;
    float y;
};

template<typename T>
using JsonEncoder = std::function<std::string(const std::string&, const T&)>;

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
        oss << to_string(vec[i]);
    }
    oss << "]";
    return oss.str();
}

template <typename T>
inline json encode_labels_with_values(const std::vector<std::string>& labels, const std::vector<T>& values)
{
    static_assert(std::is_constructible<json, std::vector<T>>::value,
        "T must be serializable to nlohmann::json");

    if (labels.size() != values.size())
    {
        throw std::invalid_argument("Labels and values vectors must have the same size.");
    }

    json j;
    j["labels"] = labels;
    j["values"] = values;
    return j;
}

inline std::string encode_signal_name_and_json(const std::string& signal, const json& value)
{
    json j;
    j["type"] = "signal";
    j["signal"] = signal;
    j["value"] = value;
    return j.dump();
}

inline std::string encode_FFT_Bands(const std::string& signal, const std::vector<float>& values)
{
    std::vector<std::string> labels = {
        "16 Hz", "20 Hz", "25 Hz", "31.5 Hz", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz",
        "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz", "500 Hz", "630 Hz", "800 Hz", "1000 Hz", "1250 Hz",
        "1600 Hz", "2000 Hz", "2500 Hz", "3150 Hz", "4000 Hz", "5000 Hz", "6300 Hz", "8000 Hz", "10000 Hz", "12500 Hz",
        "16000 Hz", "20000 Hz"
    };

    json valueJson = encode_labels_with_values(labels, values);
    return encode_signal_name_and_json(signal, valueJson);
}

template <typename T>
inline std::string encode_signal_name_and_value(const std::string& signal, const T& value)
{
    static_assert(std::is_constructible<json, T>::value,
        "T must be serializable to nlohmann::json");
    json j;
    j["type"] = "signal";
    j["signal"] = signal;
    j["value"] = value;
    return j.dump();
}

template<typename T>
JsonEncoder<T> get_signal_and_value_encoder()
{
    static_assert(std::is_constructible<json, T>::value,
        "T must be serializable to nlohmann::json");
    const JsonEncoder<T> encoder = [](const std::string& signal, const T& value) {
        json j;
        j["type"] = "signal";
        j["signal"] = signal;
        j["value"] = value;
        return j.dump();
    };
    return encoder;
}

class ISignalName
{
    public:
        virtual ~ISignalName() = default;
        virtual const std::string& GetName() const = 0;
};

template<typename T>
class ISignalValue : public ISignalName
{
    public:
        using Callback = std::function<void(const T&, void*)>;
        struct CallbackData
        {
            Callback callback;
            void* arg;
        };
        virtual void SetValue(const T& value, void* arg = nullptr) = 0;
        virtual T GetValue() const = 0;
        virtual std::shared_ptr<T> GetData() const = 0;
        virtual void RegisterCallback(Callback cb, void* arg = nullptr) = 0;
        virtual void UnregisterCallbackByArg(void* arg) = 0;
};

template<typename T>
class Signal : public ISignalValue<T>
             , public IWebSocketServer_BackendClient
             , public std::enable_shared_from_this<Signal<T>>
{
    public:
        using Callback = typename ISignalValue<T>::Callback;

        explicit Signal(const std::string& name);
        Signal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder<T> encoder = nullptr);

        void Setup();
        void SetValue(const T& value, void* arg = nullptr) override;
        T GetValue() const override;
        std::shared_ptr<T> GetData() const override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return data_;
        }
        void Notify()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            NotifyClients(nullptr);
            if (isUsingWebSocket_)
            {
                NotifyWebSocket();
            }
        }
        void RegisterCallback(Callback cb, void* arg = nullptr) override;
        void UnregisterCallbackByArg(void* arg) override;
        const std::string& GetName() const override;
        void on_message_received_from_web_socket(const std::string& message) override;

    private:
        const std::string name_;
        std::shared_ptr<T> data_;
        mutable std::mutex mutex_;
        std::vector<typename ISignalValue<T>::CallbackData> callbacks_;
        std::shared_ptr<WebSocketServer> webSocketServer_;
        std::shared_ptr<spdlog::logger> logger_;
        JsonEncoder<T> encoder_;
        bool isUsingWebSocket_;

        void NotifyClients(void* arg);
        void NotifyWebSocket();
};

class SignalManager
{
public:
    static SignalManager& GetInstance();

    template<typename T>
    std::shared_ptr<Signal<T>> CreateSignal(const std::string& name);

    template<typename T>
    std::shared_ptr<Signal<T>> CreateSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder<T> encoder = nullptr);

    ISignalName* GetSignalByName(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signals_.find(name);
        if (it != signals_.end())
        {
            return it->second.get();
        }
        return nullptr;
    }
    std::shared_ptr<ISignalName> GetSharedSignalByName(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signals_.find(name);
        if (it != signals_.end())
        {
            return it->second;
        }
        return nullptr;
    }

private:
    SignalManager();
    ~SignalManager() = default;
    SignalManager(const SignalManager&) = delete;
    SignalManager& operator=(const SignalManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<ISignalName>> signals_;
    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

#include "signal.tpp"