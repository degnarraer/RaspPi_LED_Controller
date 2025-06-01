#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <optional>

std::shared_ptr<spdlog::logger> initializeLogger(const std::string& loggerName, spdlog::level::level_enum level);

class RateLimitedLogger
{
public:
    RateLimitedLogger(std::shared_ptr<spdlog::logger> logger,
                      std::chrono::milliseconds rate_limit,
                      bool truncate = false,
                      size_t max_message_length = 200)
        : logger_(std::move(logger)),
          rate_limit_(rate_limit),
          truncate_(truncate),
          max_message_length_(max_message_length)
    {}

    template <typename... Args>
    void log(const std::string& key,
             spdlog::level::level_enum level,
             const char* fmt,
             Args&&... args)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        auto& entry = log_entries_[key];
        ++entry.count;

        if (now - entry.last_logged_time >= rate_limit_)
        {
            std::string formatted = fmt::format(fmt, std::forward<Args>(args)...);

            if (truncate_ && formatted.length() > max_message_length_)
            {
                formatted = formatted.substr(0, max_message_length_) + "...[truncated]";
            }

            logger_->log(level, "{} - Occurrence count: {}", formatted, entry.count);
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
    bool truncate_;
    size_t max_message_length_;
    std::unordered_map<std::string, LogEntry> log_entries_;
    std::mutex mutex_;
};
