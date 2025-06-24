#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_set>
#include <fstream>

class GuardDog;
class GuardDogHandler
{
public:
    static GuardDogHandler& getInstance();

    std::shared_ptr<GuardDog> createGuardDog(unsigned int timeout_seconds);
    void startMonitoringGuardDog(std::shared_ptr<GuardDog> dog);
    void stopMonitoringGuardDog(std::shared_ptr<GuardDog> dog);

    void startMonitoring();
    void stopMonitoring();

private:
    GuardDogHandler();
    ~GuardDogHandler();

    GuardDogHandler(const GuardDogHandler&) = delete;
    GuardDogHandler& operator=(const GuardDogHandler&) = delete;

    void monitorAll();

    std::vector<std::shared_ptr<GuardDog>> guarddogs_;
    std::unordered_set<std::shared_ptr<GuardDog>> active_guarddogs_;
    std::mutex mutex_;
    std::atomic<bool> monitoringActive_{false};
    std::thread monitoringThread_;
    std::ofstream watchdog_stream_;

    friend class GuardDog;
};

class GuardDog
{
public:
    GuardDog(unsigned int timeout_seconds);

    void feed();
    bool isAlive() const;

private:
    unsigned int timeout_seconds_;
    std::chrono::steady_clock::time_point last_feed_time_;
    mutable std::mutex feed_mutex_;

    friend class GuardDogHandler;
};
