#include "logger.h"

std::shared_ptr<spdlog::logger> InitializeLogger(const std::string& loggerName, spdlog::level::level_enum level)
{
    // Retrieve existing logger if available
    auto logger = spdlog::get(loggerName);
    
    if (!logger)
    {
        // Create a new colorized logger if it doesn't exist
        logger = spdlog::stdout_color_mt(loggerName);
        
        // Set the log level for this specific logger
        logger->set_level(level);
        
        // Log a message that the logger was successfully configured
        logger->info("logger configured with level {}", spdlog::level::to_str(level));
    }
    else
    {
        // If the logger already exists, ensure it's set to the correct level
        logger->set_level(level);
        logger->warn("logger already exists. Level set to {}", spdlog::level::to_str(level));
    }

    return logger;
}
