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
