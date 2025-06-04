#include "signal.h"


SignalManager& SignalManager::getInstance()
{
    static SignalManager instance;
    return instance;
}

SignalManager::SignalManager()
{
    logger_ = initializeLogger("Signal Manager", spdlog::level::info);
}

SignalName* SignalManager::getSignalByName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(signal_mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

std::shared_ptr<SignalName> SignalManager::getSharedSignalByName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(signal_mutex_);
    auto it = signals_.find(name);
    if (it != signals_.end())
    {
        return it->second;
    }
    return nullptr;
}
