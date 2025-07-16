#include "guarddog.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>

// ---------------------- GuardDog ----------------------

GuardDog::GuardDog(unsigned int timeout_seconds)
    : timeout_seconds_(timeout_seconds), last_feed_time_(std::chrono::steady_clock::now())
{
}

void GuardDog::feed()
{
    std::lock_guard<std::mutex> lock(feed_mutex_);
    last_feed_time_ = std::chrono::steady_clock::now();
}

bool GuardDog::isAlive() const
{
    std::lock_guard<std::mutex> lock(feed_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_feed_time_).count();
    return elapsed < timeout_seconds_;
}

// ------------------ GuardDogHandler -------------------

GuardDogHandler& GuardDogHandler::getInstance()
{
    static GuardDogHandler instance;
    return instance;
}

GuardDogHandler::GuardDogHandler()
{
    const std::string watchdogDevice = "/dev/watchdog";

    // Check if device exists
    if (access(watchdogDevice.c_str(), F_OK) == -1)
    {
        std::cerr << "Watchdog device not found at " << watchdogDevice << std::endl;
        std::exit(1);
    }

    watchdog_stream_.open(watchdogDevice);
    if (!watchdog_stream_.is_open())
    {
        std::cerr << "Failed to open " << watchdogDevice << std::endl;
        std::exit(1);
    }

    startMonitoring();
}

GuardDogHandler::~GuardDogHandler()
{
    stopMonitoring();
    if (watchdog_stream_.is_open())
    {
        watchdog_stream_.close();
    }
}

std::shared_ptr<GuardDog> GuardDogHandler::createGuardDog(unsigned int timeout_seconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto dog = std::make_shared<GuardDog>(timeout_seconds);
    guarddogs_.push_back(dog);
    return dog;
}

void GuardDogHandler::startMonitoringGuardDog(std::shared_ptr<GuardDog> dog)
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_guarddogs_.insert(dog);
}

void GuardDogHandler::stopMonitoringGuardDog(std::shared_ptr<GuardDog> dog)
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_guarddogs_.erase(dog);
}

void GuardDogHandler::startMonitoring()
{
    monitoringActive_ = true;
    monitoringThread_ = std::thread(&GuardDogHandler::monitorAll, this);
}

void GuardDogHandler::stopMonitoring()
{
    monitoringActive_ = false;
    if (monitoringThread_.joinable())
    {
        monitoringThread_.join();
    }
}

void GuardDogHandler::monitorAll()
{
    while (monitoringActive_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        bool allAlive = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& dog : active_guarddogs_)
            {
                if (!dog->isAlive())
                {
                    allAlive = false;
                    break;
                }
            }
        }

        if (allAlive)
        {
            watchdog_stream_ << "V";
            watchdog_stream_.flush();
        }
        else
        {
            std::cerr << "GuardDogHandler: One or more active GuardDogs not responding!" << std::endl;
            // Let the hardware watchdog reboot the system
        }
    }
}
