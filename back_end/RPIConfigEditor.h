#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <unistd.h>
#include "logger.h"

namespace fs = std::filesystem;

class RPiConfigEditor
{
public:
    RPiConfigEditor()
        : logger_(initializeLogger("ConfigEditor", spdlog::level::info))
    {
        if (!checkRoot())
        {
            logger_->error("This program must be run as root (sudo).");
            exit(1);
        }

        // Use /boot/firmware/config.txt if it exists, else fallback to /boot/config.txt
        if (fs::exists("/boot/firmware/config.txt"))
        {
            configFilePath = "/boot/firmware/config.txt";
        }
        else if (fs::exists("/boot/config.txt"))
        {
            configFilePath = "/boot/config.txt";
        }
        else
        {
            logger_->error("No config.txt file found in known locations.");
            exit(1);
        }

        logger_->info("Using config file: {}", configFilePath);
    }

    void ensureParametersEnabled(const std::vector<std::string>& dtparams,
                                 const std::vector<std::string>& dtoverlays)
    {
        addedParamsCount = 0;
        updatedParamsCount = 0;
        addedOverlaysCount = 0;

        std::ifstream infile(configFilePath);
        if (!infile.is_open())
        {
            logger_->error("Failed to open config file: {}", configFilePath);
            return;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(infile, line))
        {
            lines.push_back(line);
        }
        infile.close();

        std::vector<bool> dtparamFound(dtparams.size(), false);
        std::vector<bool> dtoverlayFound(dtoverlays.size(), false);

        // Uncomment and fix lines, mark found params/overlays
        for (auto& currentLine : lines)
        {
            std::string trimmed = trim(currentLine);
            if (trimmed.empty())
                continue;

            bool isCommented = false;
            if (trimmed[0] == '#')
            {
                isCommented = true;
                trimmed = trim(trimmed.substr(1)); // remove leading '#'
            }

            // Check dtparams
            bool matchedParam = false;
            for (size_t i = 0; i < dtparams.size(); ++i)
            {
                std::string paramPrefix = dtparams[i].substr(0, dtparams[i].find("=on"));
                if (trimmed.find(paramPrefix) == 0)
                {
                    dtparamFound[i] = true;

                    // Uncomment or fix off->on
                    if (isCommented || trimmed.find("=off") != std::string::npos)
                    {
                        currentLine = paramPrefix + "=on";
                        logger_->warn("Uncommented and/or updated {} to on", paramPrefix);
                        ++updatedParamsCount;
                    }
                    matchedParam = true;
                    break;
                }
            }

            if (matchedParam)
                continue; // skip overlay check if matched param

            // Check dtoverlays only if no dtparam matched
            for (size_t i = 0; i < dtoverlays.size(); ++i)
            {
                if (trimmed == dtoverlays[i])
                {
                    dtoverlayFound[i] = true;
                    // Uncomment overlays if commented
                    if (isCommented)
                    {
                        currentLine = dtoverlays[i];
                        logger_->warn("Uncommented dtoverlay: {}", dtoverlays[i]);
                        ++updatedParamsCount;
                    }
                    break;
                }
            }
        }

        // Add missing dtparams
        for (size_t i = 0; i < dtparams.size(); ++i)
        {
            if (!dtparamFound[i])
            {
                lines.push_back(dtparams[i]);
                logger_->info("Added missing dtparam: {}", dtparams[i]);
                ++addedParamsCount;
            }
        }

        // Add missing dtoverlays
        for (size_t i = 0; i < dtoverlays.size(); ++i)
        {
            if (!dtoverlayFound[i])
            {
                lines.push_back(dtoverlays[i]);
                logger_->info("Added missing dtoverlay: {}", dtoverlays[i]);
                ++addedOverlaysCount;
            }
        }

        // Remove duplicates, keeping first occurrence
        std::unordered_set<std::string> seenLines;
        std::vector<std::string> cleanedLines;

        for (const auto& l : lines)
        {
            std::string trimmed = trim(l);
            if (trimmed.empty())
                continue;

            // Normalize line by removing leading '#' if any
            std::string normalized = trimmed[0] == '#' ? trim(trimmed.substr(1)) : trimmed;

            if (seenLines.find(normalized) == seenLines.end())
            {
                seenLines.insert(normalized);
                cleanedLines.push_back(l);
            }
            else
            {
                logger_->info("Removed duplicate line: {}", l);
            }
        }

        lines = std::move(cleanedLines);

        std::ofstream outfile(configFilePath);
        if (!outfile.is_open())
        {
            logger_->error("Failed to open config file for writing: {}", configFilePath);
            return;
        }

        for (const auto& outputLine : lines)
        {
            outfile << outputLine << std::endl;
        }
        outfile.close();

        logger_->info("Status Summary:\n{}", getStatusSummary());
        tryRebootSystem();
    }

    std::string getStatusSummary() const
    {
        std::string summary;
        summary += "Params added: " + std::to_string(addedParamsCount) + "\n";
        summary += "Params updated: " + std::to_string(updatedParamsCount) + "\n";
        summary += "Overlays added: " + std::to_string(addedOverlaysCount);
        return summary;
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::string configFilePath;

    int addedParamsCount{0};
    int updatedParamsCount{0};
    int addedOverlaysCount{0};

    static bool checkRoot()
    {
        return geteuid() == 0;
    }

    void tryRebootSystem()
    {
        if (addedParamsCount == 0 && updatedParamsCount == 0 && addedOverlaysCount == 0)
        {
            logger_->info("No changes were necessary.");
        }
        else
        {
            logger_->info("Rebooting system now...");
            int ret = std::system("reboot");
            if (ret != 0)
            {
                logger_->error("Failed to reboot system, return code: {}", ret);
            }
            // The system should reboot immediately, so this function may not return.
        }
    }

    static std::string trim(const std::string& s)
    {
        auto start = s.begin();
        while (start != s.end() && std::isspace(*start))
        {
            start++;
        }

        auto end = s.end();
        do
        {
            end--;
        } while (std::distance(start, end) > 0 && std::isspace(*end));

        return std::string(start, end + 1);
    }
};
