#pragma once
#include <spdlog/spdlog.h>
#include <string>
#include <memory>
#include <chrono>

std::shared_ptr<spdlog::logger> InitializeLogger(const std::string& loggerName, spdlog::level::level_enum level);

#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <mutex>

std::shared_ptr<spdlog::logger> InitializeLogger(const std::string& loggerName, spdlog::level::level_enum level);

class RateLimitedLogger
{
public:
    RateLimitedLogger(std::shared_ptr<spdlog::logger> logger, std::chrono::milliseconds rate_limit)
        : logger_(std::move(logger)), rate_limit_(rate_limit)
    {}

    template <typename... Args>    
    void log(const std::string& key, spdlog::level::level_enum level, const char* fmt, Args&&... args)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        auto& entry = log_entries_[key];
        ++entry.count;

        if (now - entry.last_logged_time >= rate_limit_)
        {
            logger_->log(level, "{} - Occurrence count: {}", fmt::format(fmt, std::forward<Args>(args)...), entry.count);
            entry.last_logged_time = now;
            entry.count = 0;
        }
    }

private:
    struct LogEntry
    {
        std::chrono::steady_clock::time_point last_logged_time{std::chrono::steady_clock::now()};
        int count{0};
    };

    std::shared_ptr<spdlog::logger> logger_;
    std::chrono::milliseconds rate_limit_;
    std::unordered_map<std::string, LogEntry> log_entries_;
    std::mutex mutex_;
};
