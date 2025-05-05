#include "signal.h"


SignalManager& SignalManager::GetInstance()
{
    static SignalManager instance;
    return instance;
}

SignalManager::SignalManager()
{
    logger_ = InitializeLogger("Signal Manager", spdlog::level::info);
}
