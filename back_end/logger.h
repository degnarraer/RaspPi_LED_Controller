#pragma once
#include <spdlog/spdlog.h>
#include <string>
#include <memory>
#include <chrono>

std::shared_ptr<spdlog::logger> InitializeLogger(const std::string& loggerName, spdlog::level::level_enum level);

class RateLimitedLogger
{
public:
    RateLimitedLogger(std::shared_ptr<spdlog::logger> logger, std::chrono::milliseconds rate_limit)
        : logger_(std::move(logger)), rate_limit_(rate_limit), last_logged_time_(std::chrono::steady_clock::now()), count_(0)
    {}

    template <typename... Args>
    void log(spdlog::level::level_enum level, const char* fmt, Args&&... args)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - last_logged_time_ >= rate_limit_)
        {
            logger_->log(level, "{} - Occurrence count: {}", fmt::format(fmt, std::forward<Args>(args)...), ++count_);
            last_logged_time_ = now;
        }
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::chrono::milliseconds rate_limit_;
    std::chrono::steady_clock::time_point last_logged_time_;
    int count_;
};