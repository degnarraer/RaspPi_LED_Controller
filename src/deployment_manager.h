#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

// Function to deploy index.html with sudo privileges
void copyFolderContents(const std::string& sourceDir, const std::string& targetDir) {
    try {
        // Check if source directory exists
        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
            std::cerr << "Source directory does not exist or is not a directory." << std::endl;
            return;
        }

        // Check if target directory exists, if not, create it
        if (!fs::exists(targetDir)) {
            fs::create_directories(targetDir);
        }

        // Iterate over the source directory and copy files to target directory
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            const auto& sourcePath = entry.path();
            auto targetPath = fs::path(targetDir) / sourcePath.filename();  // Target path for file

            if (fs::is_directory(sourcePath)) {
                // Recursively copy contents of subdirectories
                copyFolderContents(sourcePath.string(), targetPath.string());
            } else if (fs::is_regular_file(sourcePath)) {
                // Copy file to target directory, replacing if necessary
                std::cout << "Copying " << sourcePath << " to " << targetPath << std::endl;
                fs::copy(sourcePath, targetPath, fs::copy_options::overwrite_existing);
            }
        }

        std::cout << "All files copied successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
void copyFolderContentsWithSudo(const std::string& sourceDir, const std::string& targetDir) {
    try {
        // Check if source directory exists
        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
            std::cerr << "Source directory does not exist or is not a directory." << std::endl;
            return;
        }

        // Check if target directory exists, if not, create it using sudo
        if (!fs::exists(targetDir)) {
            std::string createCmd = "sudo mkdir -p " + targetDir;
            if (system(createCmd.c_str()) != 0) {
                std::cerr << "Failed to create target directory with sudo!" << std::endl;
                return;
            }
        }

        // Iterate over the source directory and copy files to the target directory using sudo
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            const auto& sourcePath = entry.path();
            auto targetPath = fs::path(targetDir) / sourcePath.filename();  // Target path for file

            if (fs::is_directory(sourcePath)) {
                // Recursively copy contents of subdirectories using sudo
                copyFolderContentsWithSudo(sourcePath.string(), targetPath.string());
            } else if (fs::is_regular_file(sourcePath)) {
                // Copy file to target directory, replacing if necessary using sudo
                std::cout << "Copying " << sourcePath << " to " << targetPath << std::endl;
                std::string copyCmd = "sudo cp -f " + sourcePath.string() + " " + targetPath.string();
                if (system(copyCmd.c_str()) != 0) {
                    std::cerr << "Failed to copy file: " << sourcePath << std::endl;
                    return;
                }
            }
        }

        std::cout << "All files copied successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
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


std::string executeCommand(const std::string& cmd) {
    char buffer[128];
    std::string result = ""; 
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen failed!");
    }
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

// Function to check if a package is installed
bool isPackageInstalled(const std::string& packageName) {
    std::string cmd = "dpkg -l | grep " + packageName;
    std::string output = executeCommand(cmd);
    return !output.empty();  // If there's any output, the package is installed
}

// Function to install a package if not installed
void installPackageIfNeeded(const std::string& packageName) {
    if (isPackageInstalled(packageName)) {
        std::cout << packageName << " is already installed.\n";
    } else {
        std::cout << packageName << " is not installed. Installing...\n";
        std::string installCmd = "sudo apt-get install -y " + packageName;
        int result = system(installCmd.c_str());
        if (result == 0) {
            std::cout << packageName << " installed successfully.\n";
        } else {
            std::cerr << "Error installing " << packageName << "\n";
        }
    }
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

void startAndEnableNginx() {
    // Start the nginx service
    std::cout << "Starting nginx..." << std::endl;
    int startResult = system("sudo systemctl start nginx");
    if (startResult != 0) {
        std::cerr << "Failed to start nginx!" << std::endl;
        return;
    }

    // Enable nginx to start on boot
    std::cout << "Enabling nginx to start on boot..." << std::endl;
    int enableResult = system("sudo systemctl enable nginx");
    if (enableResult != 0) {
        std::cerr << "Failed to enable nginx on boot!" << std::endl;
        return;
    }

    std::cout << "NGINX started and enabled successfully!" << std::endl;
}

/*
int main() {
    startAndEnableNginx();
    return 0;
}
*/