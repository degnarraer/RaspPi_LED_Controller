#include <fstream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <sstream>
#include <cstdio>
#include <sys/statvfs.h>
#include "logger.h"
#include "signals/signal.h"

class SystemStatusMonitor
{
public:
    SystemStatusMonitor(std::shared_ptr<WebSocketServer> webSocketServer)
        : webSocketServer_(webSocketServer)
        , logger_(initializeLogger("SystemStatusMonitor", spdlog::level::info))
        , cpuUsageSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("CPU Usage")))
        , memoryUsageSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("CPU Memory Usage")))
        , cpuTempSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("CPU Temp")))
        , gpuTempSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("GPU Temp")))
        , throttleStatusSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Throttle Status")))
        , netRxSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Network RX")))
        , netTxSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Network TX")))
        , diskUsageSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Disk Usage")))
        , loadAvgSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Load Average")))
        , uptimeSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Uptime")))
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
    std::shared_ptr<Signal<std::string>> gpuTempSignal_;
    std::shared_ptr<Signal<std::string>> throttleStatusSignal_;
    std::shared_ptr<Signal<std::string>> netRxSignal_;
    std::shared_ptr<Signal<std::string>> netTxSignal_;
    std::shared_ptr<Signal<std::string>> diskUsageSignal_;
    std::shared_ptr<Signal<std::string>> loadAvgSignal_;
    std::shared_ptr<Signal<std::string>> uptimeSignal_;
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

    float getGpuTemperature()
    {
        std::ifstream file("/sys/class/thermal/thermal_zone1/temp");
        if (!file)
        {
            logger_->error("Failed to open GPU temperature file");
            return 0.0f;
        }

        int tempMilliCelsius = 0;
        file >> tempMilliCelsius;
        file.close();

        float tempCelsius = tempMilliCelsius / 1000.0f;
        logger_->debug("GPU temp: {} °C", tempCelsius);
        return tempCelsius;
    }

    std::string getThrottleStatus()
    {
        std::ifstream file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
        if (!file)
        {
            logger_->error("Failed to open scaling governor file");
            return "Unknown";
        }

        std::string status;
        std::getline(file, status);
        file.close();
        logger_->debug("Throttle status: {}", status);
        return status;
    }

    void getNetworkStats(float &rx, float &tx)
    {
        std::ifstream file("/proc/net/dev");
        if (!file)
        {
            logger_->error("Failed to open /proc/net/dev");
            rx = tx = 0.0f;
            return;
        }

        std::string line;
        rx = tx = 0.0f;

        while (std::getline(file, line))
        {
            if (line.find("eth0:") != std::string::npos) // Assuming eth0 is the network interface
            {
                std::istringstream ss(line);
                std::string iface;
                ss >> iface >> rx >> tx;
                break;
            }
        }

        file.close();
        logger_->debug("Network RX: {} bytes, TX: {} bytes", rx, tx);
    }

    float getDiskUsage()
    {
        struct statvfs buf;  // Now the type is defined
        if (statvfs("/", &buf) != 0)
        {
            logger_->error("Failed to get disk usage");
            return 0.0f;
        }

        float usage = (float)(buf.f_blocks - buf.f_bfree) / buf.f_blocks * 100.0f;
        logger_->debug("Disk usage: {}%", usage);
        return usage;
    }

    std::string getLoadAverage()
    {
        std::ifstream file("/proc/loadavg");
        if (!file)
        {
            logger_->error("Failed to open /proc/loadavg");
            return "Unknown";
        }

        std::string loadAvg;
        file >> loadAvg;
        file.close();
        logger_->debug("Load average: {}", loadAvg);
        return loadAvg;
    }

    std::string getUptime()
    {
        std::ifstream file("/proc/uptime");
        if (!file)
        {
            logger_->error("Failed to open /proc/uptime");
            return "Unknown";
        }

        std::string uptime;
        file >> uptime;
        file.close();
        logger_->debug("Uptime: {}", uptime);
        return uptime;
    }

    void updateSystemStats()
    {
        logger_->debug("Updating system status...");

        // Update signals and log results
        float valueF;
        std::string valueS;

        // Update CPU temperature
        valueF = getCpuTemperature();
        if (cpuTempSignal_)
        {
            valueS = fmt::format("{:.2f} °C", valueF);
            cpuTempSignal_->setValue(valueS);
            logger_->debug("CPU Temp: {}", valueS);
        }

        // Update CPU usage
        valueF = getCpuUsage();
        if (cpuUsageSignal_)
        {
            valueS = fmt::format("{:.2f} %", valueF);
            cpuUsageSignal_->setValue(valueS);
            logger_->debug("CPU Usage: {}", valueS);
        }

        // Update memory usage
        valueF = getMemoryUsage();
        if (memoryUsageSignal_)
        {
            valueS = fmt::format("{:.2f} %", valueF);
            memoryUsageSignal_->setValue(valueS);
            logger_->debug("Memory Usage: {}", valueS);
        }

        // Update GPU temperature
        //valueF = getGpuTemperature();
        if (gpuTempSignal_)
        {
            valueS = fmt::format("{:.2f} °C", valueF);
            gpuTempSignal_->setValue(valueS);
            logger_->debug("GPU Temp: {}", valueS);
        }

        // Update throttle status
        valueS = getThrottleStatus();
        if (throttleStatusSignal_)
        {
            throttleStatusSignal_->setValue(valueS);
            logger_->debug("Throttle Status: {}", valueS);
        }

        // Update network stats
        float rx, tx;
        getNetworkStats(rx, tx);
        if (netRxSignal_)
        {
            valueS = fmt::format("{:.2f} KB", rx);
            netRxSignal_->setValue(valueS);
            logger_->debug("Net RX: {}", valueS);
        }
        if (netTxSignal_)
        {
            valueS = fmt::format("{:.2f} KB", tx);
            netTxSignal_->setValue(valueS);
            logger_->debug("Net TX: {}", valueS);
        }

        // Update disk usage
        valueF = getDiskUsage();
        if (diskUsageSignal_)
        {
            valueS = fmt::format("{:.2f} %", valueF);
            diskUsageSignal_->setValue(valueS);
            logger_->debug("Disk Usage: {}", valueS);
        }

        // Update load average
        valueS = getLoadAverage();
        if (loadAvgSignal_)
        {
            loadAvgSignal_->setValue(valueS);
            logger_->debug("Load Average: {}", valueS);
        }

        // Update uptime
        valueS = getUptime();
        if (uptimeSignal_)
        {
            uptimeSignal_->setValue(valueS);
            logger_->debug("Uptime: {}", valueS);
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
