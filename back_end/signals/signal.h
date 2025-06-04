#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <type_traits>
#include "../logger.h"
#include "../websocket_server.h"

using json = nlohmann::json;

struct Point
{
    float x;
    float y;
};

template<typename T>
using JsonEncoder = std::function<std::string(const std::string&, const T&)>;

enum class BinaryEncoderType : uint8_t
{
    /**
     * Named_Binary_Encoder (0x01)
     *
     * Binary layout:
     * --------------------------------------------------------
     * | Offset | Field        | Size    | Description         |
     * |--------|--------------|---------|---------------------|
     * | 0      | message_type | 1 byte  | Always 0x01         |
     * | 1–2    | name_length  | 2 bytes | Big-endian uint16_t |
     * | 3–N    | signal_name  | N bytes | UTF-8, not null-term|
     * | N+1+   | payload      | varies  | Signal value data   |
     * 
     * Notes:
     * - Byte order: All multi-byte fields use **big-endian**.
     * - signal_name: Encoded as raw UTF-8, no null terminator.
     * - payload: Binary blob of the signal's value, e.g., RGB matrix.
     * - Extensible by using new values for `BinaryEncoderType`.
     */
    Named_Binary_Encoder = 1,
    
    /**
     * Timestamped_Int_Vector_Encoder (0x02)
     *
     * Binary layout:
     * ------------------------------------------------------------
     * | Offset | Field        | Size         | Description        |
     * |--------|--------------|--------------|--------------------|
     * | 0      | message_type | 1 byte       | Always 0x02        |
     * | 1–2    | name_length  | 2 bytes      | Big-endian uint16_t|
     * | 3–N    | signal_name  | N bytes      | UTF-8              |
     * | N+1+   | timestamp    | 8 bytes      | Big-endian uint64_t|
     * | N+9+   | vector_len   | 4 bytes      | Big-endian uint32_t|
     * | N+13+  | vector_data  | 4 * len bytes| int32_t values     |
     *
     * Notes:
     * - Timestamp is in microseconds since epoch.
     * - All integers are big-endian.
     */
    Timestamped_Int_Vector_Encoder = 2,
};

template<typename T>
using BinaryEncoder = std::function<std::vector<uint8_t>(const std::string&, const T&)>;

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
    return encode_signal_name_and_json(signal, encode_labels_with_values(labels, values));
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

class SignalName
{
    public:
        SignalName(const std::string name) : name_(name)
                                           , logger_(initializeLogger("Signal " + name + "Logger", spdlog::level::info)) {}

        virtual ~SignalName() = default;
        const std::string& getName() const
        {
            return name_;
        }
    protected:
        const std::string name_;
        std::shared_ptr<spdlog::logger> logger_;
};

template<typename T>
class SignalValue: public SignalName
{
    public:
        SignalValue(const std::string& name) : SignalName(name)
                                             , data_(std::make_shared<T>()) {}
        using SignalValueCallback = std::function<void(const T&, void*)>;
        struct SignalValueCallbackData
        {
            SignalValueCallback callback;
            void* arg;
        };
        virtual bool setValue(const T& value, void* arg = nullptr);
        virtual T getValue() const;
        virtual std::shared_ptr<T> GetData() const
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            return data_;
        }
        void registerSignalValueCallback(SignalValueCallback cb, void* arg = nullptr);
        void unregisterSignalValueCallbackByArg(void* arg);

        virtual ~SignalValue() = default;
    protected:
        std::shared_ptr<T> data_;
        mutable std::mutex dataMutex_;
        std::vector<typename SignalValue<T>::SignalValueCallbackData> callbacks_;
        mutable std::mutex callbackMutex_;
};

template<typename T>
class Signal : public SignalValue<T>
             , public WebSocketServerNotificationClient
             , public std::enable_shared_from_this<Signal<T>>
{
    public:
        using SignalValueCallback = typename SignalValue<T>::SignalValueCallback;

        explicit Signal(const std::string& name);
        Signal( const std::string& name
              , std::weak_ptr<WebSocketServer> webSocketServer
              , JsonEncoder<T> encoder = nullptr
              , MessagePriority priority = MessagePriority::Low
              , bool should_retry = false );
        Signal( const std::string& name
              , std::weak_ptr<WebSocketServer> webSocketServer
              , BinaryEncoder<T> encoder = nullptr
              , MessagePriority priority = MessagePriority::Low
              , bool should_retry = false );

        void setup();
        bool setValue(const T& value, void* arg = nullptr) override;
        void notify()
        {
            std::lock_guard<std::mutex> lock(this->dataMutex_);
            notifyClients(nullptr);
            if (isUsingWebSocket_)
            {
                notifyWebSocket();
            }
        }

        // WebSocketServerNotificationClient Interface
        const std::string& getName() const override
        {
            return this->name_;
        }
        void onValueRequest() const override
        {
            if (this->logger_)
            {
                this->logger_->debug("WebSocketServerNotificationClient: Value requested for signal {}", this->name_);
            }
        }
    private:
        std::weak_ptr<WebSocketServer> webSocketServer_;
        JsonEncoder<T> jsonEncoder_;
        BinaryEncoder<T> binaryEncoder_;
        MessagePriority priority_;
        bool should_retry_;
        bool isUsingWebSocket_;
        void notifyClients(void* arg);
        void notifyWebSocket();
};

class SignalManager
{
public:
    static SignalManager& getInstance();

    template<typename T>
    std::shared_ptr<Signal<T>> createSignal(const std::string& name);

    template<typename T>
    std::shared_ptr<Signal<T>> createSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, JsonEncoder<T> encoder = nullptr);
    
    template<typename T>
    std::shared_ptr<Signal<T>> createSignal(const std::string& name, std::shared_ptr<WebSocketServer> webSocketServer, BinaryEncoder<T> encoder = nullptr);

    SignalName* getSignalByName(const std::string& name);
    std::shared_ptr<SignalName> getSharedSignalByName(const std::string& name);

private:
    SignalManager();
    ~SignalManager() = default;
    SignalManager(const SignalManager&) = delete;
    SignalManager& operator=(const SignalManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<SignalName>> signals_;
    std::mutex signal_mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

#include "signal.tpp"