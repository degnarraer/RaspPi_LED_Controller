#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include "../logger.h"
#include "../websocket_server.h"
#include "DataTypesAndEncoders/DataTypesAndEncoders.h"

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

        //Functions to handle at Signal Name Level
        virtual bool setValueFromJSON(const json& j) = 0;
        virtual bool handleWebSocketValueRequest() const = 0;

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
        virtual bool setValueFromJSON(const json& j) override;
        virtual bool handleWebSocketValueRequest() const override
        {
            this->logger_->debug("SignalValue: Value requested for signal {} not handled", this->name_);
            return false;
        }
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

        bool handleWebSocketValueRequest() const override
        {
            this->logger_->info("Handle Value requeste for signal \"{}\"", this->name_);
            return notifyWebSocket();
        }
    private:
        std::weak_ptr<WebSocketServer> webSocketServer_;
        JsonEncoder<T> jsonEncoder_;
        BinaryEncoder<T> binaryEncoder_;
        MessagePriority priority_;
        bool should_retry_;
        bool isUsingWebSocket_;
        bool notifyClients(void* arg) const;
        bool notifyWebSocket() const;
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