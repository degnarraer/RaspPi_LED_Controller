#include <fstream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <sstream>
#include <cstdio>
#include "logger.h"
#include "signal.h"

class SystemStatusMonitor
{
public:
    SystemStatusMonitor( std::shared_ptr<WebSocketServer> webSocketServer)
                       : webSocketServer_(webSocketServer)
                       , logger_(InitializeLogger("SystemStatusMonitor", spdlog::level::debug))
                       , cpuUsageSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::GetInstance().GetSharedSignalByName("CPU Usage")))
                       , memoryUsageSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::GetInstance().GetSharedSignalByName("Memory Usage")))
                       , cpuTempSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::GetInstance().GetSharedSignalByName("CPU Temp")))
                       , running_(false)
    {
        logger_->info("SystemStatusMonitor initialized.");
    }

    ~SystemStatusMonitor()
    {
        stopMonitoring();
    }

    void startMonitoring()
    {
        if (running_)
        {
            logger_->warn("Monitoring is already running.");
            return;
        }

        logger_->info("Starting monitoring thread...");
        running_ = true;
        monitoringThread_ = std::thread(&SystemStatusMonitor::monitoringLoop, this);
    }

    void stopMonitoring()
    {
        if (!running_)
        {
            logger_->warn("Monitoring is not running.");
            return;
        }

        logger_->info("Stopping monitoring thread...");
        running_ = false;
        if (monitoringThread_.joinable())
        {
            monitoringThread_.join();
        }
        logger_->info("Monitoring thread stopped.");
    }

private:
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::shared_ptr<Signal<std::string>> cpuUsageSignal_;
    std::shared_ptr<Signal<std::string>> memoryUsageSignal_;
    std::shared_ptr<Signal<std::string>> cpuTempSignal_;
    std::shared_ptr<spdlog::logger> logger_;
    bool running_;
    std::thread monitoringThread_;

    float getCpuUsage()
    {
        std::ifstream file("/proc/stat");
        if (!file)
        {
            logger_->error("Failed to open /proc/stat");
            return 0.0f;
        }

        std::string line;
        std::getline(file, line);
        file.close();

        long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
        if (std::sscanf(line.c_str(), "cpu  %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice) != 10)
        {
            logger_->error("Failed to parse /proc/stat");
            return 0.0f;
        }

        static long prev_total = 0, prev_idle = 0;
        long total = user + nice + system + idle + iowait + irq + softirq + steal;
        long idle_all = idle + iowait;

        long delta_total = total - prev_total;
        long delta_idle = idle_all - prev_idle;

        prev_total = total;
        prev_idle = idle_all;

        if (delta_total == 0) return 0.0f;

        float usage = (1.0f - (float)delta_idle / delta_total) * 100.0f;
        logger_->debug("CPU usage calculated: {}%", usage);
        return usage;
    }

    float getMemoryUsage()
    {
        std::ifstream file("/proc/meminfo");
        if (!file)
        {
            logger_->error("Failed to open /proc/meminfo");
            return 0.0f;
        }

        std::string line;
        long totalMemory = 0, freeMemory = 0, buffers = 0, cached = 0;

        while (std::getline(file, line))
        {
            if (line.find("MemTotal:") == 0)
                std::sscanf(line.c_str(), "MemTotal: %ld kB", &totalMemory);
            else if (line.find("MemFree:") == 0)
                std::sscanf(line.c_str(), "MemFree: %ld kB", &freeMemory);
            else if (line.find("Buffers:") == 0)
                std::sscanf(line.c_str(), "Buffers: %ld kB", &buffers);
            else if (line.find("Cached:") == 0)
                std::sscanf(line.c_str(), "Cached: %ld kB", &cached);
        }
        file.close();

        if (totalMemory == 0)
        {
            logger_->error("Invalid total memory (0)");
            return 0.0f;
        }

        long used = totalMemory - freeMemory - buffers - cached;
        float usage = (float)used / totalMemory * 100.0f;
        logger_->debug("Memory usage calculated: {}%", usage);
        return usage;
    }

    float getCpuTemperature()
    {
        std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
        if (!file)
        {
            logger_->error("Failed to open /sys/class/thermal/thermal_zone0/temp");
            return 0.0f;
        }

        int tempMilliCelsius = 0;
        file >> tempMilliCelsius;
        file.close();

        float tempCelsius = tempMilliCelsius / 1000.0f;
        logger_->debug("CPU temp: {} °C", tempCelsius);
        return tempCelsius;
    }

    void updateSystemStats()
    {
        logger_->debug("Updating signal values...");
        try
        {
            if (cpuTempSignal_)
            {
                float temp = getCpuTemperature();
                cpuTempSignal_->SetValue(fmt::format("{:.2f} °C", temp));
            }
        }
        catch (const std::exception& e)
        {
            logger_->error("Exception updating CPU temp: {}", e.what());
        }

        try
        {
            if (cpuUsageSignal_)
            {
                float cpu = getCpuUsage();
                cpuUsageSignal_->SetValue(fmt::format("{:.2f} %", cpu));
            }
        }
        catch (const std::exception& e)
        {
            logger_->error("Exception updating CPU usage: {}", e.what());
        }

        try
        {
            if (memoryUsageSignal_)
            {
                float mem = getMemoryUsage();
                memoryUsageSignal_->SetValue(fmt::format("{:.2f} %", mem));
            }
        }
        catch (const std::exception& e)
        {
            logger_->error("Exception updating memory usage: {}", e.what());
        }
    }

    void monitoringLoop()
    {
        logger_->info("Monitoring thread running.");
        while (running_)
        {
            try
            {
                updateSystemStats();
            }
            catch (const std::exception& e)
            {
                logger_->error("Exception in monitoring loop: {}", e.what());
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        logger_->info("Monitoring thread exiting.");
    }
};
