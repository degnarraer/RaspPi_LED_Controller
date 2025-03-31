#pragma once
#include <spdlog/spdlog.h>
#include <string>
#include <memory>

std::shared_ptr<spdlog::logger> InitializeLogger(const std::string& loggerName, spdlog::level::level_enum level);
