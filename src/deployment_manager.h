#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

class DeploymentManager
{
    public:
        DeploymentManager()
        {
            // Retrieve existing logger or create a new one
            logger = spdlog::get("Deployment Manager");
            if (!logger)
            {
                logger = spdlog::stdout_color_mt("Deployment Manager");
                spdlog::register_logger(logger);
            }
        }
        ~DeploymentManager(){}
        std::shared_ptr<spdlog::logger> logger;

        // Function to clear all contents from a folder without sudo
        void clearFolderContents(const std::string& folderPath)
        {
            try
            {
                if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
                {
                    logger->error("Folder does not exist or is not a directory: {}", folderPath);
                    return;
                }

                for (const auto& entry : fs::directory_iterator(folderPath))
                {
                    try
                    {
                        fs::remove_all(entry.path());
                    }
                    catch (const std::exception& e)
                    {
                        //logger->error("Error removing file {}: {}", entry.path().string(), e.what());
                    }
                }
                logger->info("All contents removed from: {}", folderPath);
            }
            catch (const std::exception& e)
            {
                logger->error("Error clearing folder: {}", e.what());
            }
        }

        // Function to clear all contents from a folder using sudo
        void clearFolderContentsWithSudo(const std::string& folderPath)
        {
            std::string cmd = "sudo rm -rf " + folderPath + "/*";
            int result = system(cmd.c_str());
            if (result == 0)
            {
                logger->info("All contents removed from: {} successfully (with sudo).", folderPath);
            }
            else
            {
                logger->error("Failed to clear contents of {} using sudo.", folderPath);
            }
        }

        // Function to deploy index.html with sudo privileges
        void copyFolderContents(const std::string& sourceDir, const std::string& targetDir)
        {
            try 
            {
                // Check if source directory exists
                if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir))
                {
                    logger->error("Source directory {} does not exist or is not a directory.", sourceDir);
                    return;
                }

                // Check if target directory exists, if not, create it
                if (!fs::exists(targetDir))
                {
                    fs::create_directories(targetDir);
                }

                // Iterate over the source directory and copy files to target directory
                for (const auto& entry : fs::directory_iterator(sourceDir))
                {
                    const auto& sourcePath = entry.path();
                    auto targetPath = fs::path(targetDir) / sourcePath.filename();  // Target path for file
                    
                    std::string sourcePathStr = sourcePath.string();
                    std::string targetPathStr = targetPath.string();

                    if (fs::is_directory(sourcePath))
                    {
                        // Recursively copy contents of subdirectories
                        copyFolderContents(sourcePathStr, targetPathStr);
                    }
                    else if (fs::is_regular_file(sourcePath))
                    {
                        // Copy file to target directory, replacing if necessary
                        logger->info("Copying {} to {}.", sourcePathStr, targetPathStr);
                        fs::copy(sourcePath, targetPath, fs::copy_options::overwrite_existing);
                    }
                }
                logger->info("All files copied successfully!");
            } 
            catch (const std::exception& e) 
            {
                logger->info("Error: {}", e.what());
            }
        }

        void copyFolderContentsWithSudo(const std::string &sourceDir, const std::string &targetDir)
        {
            try
            {
                if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir))
                {
                    logger->error("Source directory {} does not exist or is not a directory.", sourceDir);
                    return;
                }

                if (!fs::exists(targetDir))
                {
                    std::string createCmd = "sudo mkdir -p " + targetDir;
                    int createResult = system(createCmd.c_str());
                    if (createResult != 0)
                    {
                        logger->error("Failed to create target directory with sudo: {}", targetDir);
                        return;
                    }
                }

                for (const auto &entry : fs::directory_iterator(sourceDir))
                {
                    const auto &sourcePath = entry.path();
                    auto targetPath = fs::path(targetDir) / sourcePath.filename();
                    
                    std::string sourcePathStr = sourcePath.string();
                    std::string targetPathStr = targetPath.string();
                    
                    if (fs::is_directory(sourcePath))
                    {
                        copyFolderContentsWithSudo(sourcePathStr, targetPathStr);
                    }
                    else if (fs::is_regular_file(sourcePath))
                    {
                        std::string copyCmd = "sudo cp -f " + sourcePathStr + " " + targetPathStr;
                        int copyResult = system(copyCmd.c_str());
                        if (copyResult != 0)
                        {
                            logger->error("Failed to copy file: {} to {}", sourcePathStr, targetPathStr);
                            return;
                        }
                    }
                }
                logger->info("All files copied successfully (with sudo)!");
            }
            catch (const std::exception &e)
            {
                logger->error("Error copying folder contents with sudo: {}", e.what());
            }
        }

        /*
        int main() {
            std::string sourceFolder = "/path/to/source"; // Replace with your source folder path
            std::string targetFolder = "/path/to/target"; // Replace with your target folder path
            copyFolderContents(sourceFolder, targetFolder);
            return 0;
        }
        */

        std::string executeCommand(const std::string &cmd)
        {
            char buffer[128];
            std::string result;
            std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe)
            {
                throw std::runtime_error("popen failed!");
            }
            while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
            {
                result += buffer;
            }
            return result;
        }

        bool isPackageInstalled(const std::string &packageName)
        {
            std::string cmd = "dpkg -l | grep " + packageName;
            std::string output = executeCommand(cmd);
            return !output.empty();
        }

        void installPackageIfNeeded(const std::string &packageName)
        {
            if (isPackageInstalled(packageName))
            {
                logger->info("{} is already installed.", packageName);
            }
            else
            {
                logger->info("{} is not installed. Installing...", packageName);
                std::string installCmd = "sudo apt-get install -y " + packageName;
                int result = system(installCmd.c_str());
                if (result == 0)
                {
                    logger->info("{} installed successfully.", packageName);
                }
                else
                {
                    logger->error("Error installing {}.", packageName);
                }
            }
        }

        void startAndEnableNginx()
        {
            logger->info("Starting nginx...");
            int startResult = system("sudo systemctl start nginx");
            if (startResult != 0)
            {
                logger->error("Failed to start nginx!");
                return;
            }

            logger->info("Enabling nginx to start on boot...");
            int enableResult = system("sudo systemctl enable nginx");
            if (enableResult != 0)
            {
                logger->error("Failed to enable nginx on boot!");
                return;
            }

            logger->info("NGINX started and enabled successfully!");
        }
        /*
        int main() {
            // List of packages to check and install if needed
            std::string packages[] = {"libasound2-dev", "cmake", "nginx"};

            // Check and install each package
            for (const auto& package : packages) {
                installPackageIfNeeded(package);
            }

            return 0;
        }
        */
};