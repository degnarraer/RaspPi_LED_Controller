#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <cstdlib>

class GuardDogHandler;

class GuardDog
{
public:
    GuardDog(GuardDogHandler& handler, unsigned int timeout_seconds)
        : handler_(handler), timeout_seconds_(timeout_seconds), is_active_(true)
    {
        // Start the monitoring thread
        guarddog_thread_ = std::thread(&GuardDog::monitor, this);
    }

    ~GuardDog()
    {
        if (guarddog_thread_.joinable())
        {
            guarddog_thread_.join();
        }
    }

    // Feed the GuardDog (reset the timer)
    void feed()
    {
        last_reset_time_ = std::chrono::steady_clock::now();
    }

    // Stop the GuardDog monitoring (e.g., if the thread is complete)
    void leash()
    {
        
        is_active_ = false;
    }

    // Activate the GuardDog and restart monitoring
    void unleash()
    {
        if (!is_active_)
        {
            is_active_ = true;
            last_reset_time_ = std::chrono::steady_clock::now(); // reset timer
            guarddog_thread_ = std::thread(&GuardDog::monitor, this); // restart monitoring thread
        }
    }

    // Check if the GuardDog is still active
    bool isActive() const
    {
        return is_active_;
    }

private:
    void monitor()
    {
        while (is_active_)
        {
            std::this_thread::sleep_for(std::chrono::seconds(timeout_seconds_));
            auto now = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_reset_time_).count();

            if (elapsed_time >= timeout_seconds_)
            {
                std::cout << "GuardDog timeout! Rebooting...\n";
                // Reboot the system (simulated here)
                std::exit(1);
            }

            // Feed the hardware watchdog periodically (for real hardware)
            handler_.feedHardwareWatchdog();
        }
    }

    GuardDogHandler& handler_;
    unsigned int timeout_seconds_;
    std::chrono::steady_clock::time_point last_reset_time_ = std::chrono::steady_clock::now();
    std::atomic<bool> is_active_;
    std::thread guarddog_thread_;
};

class GuardDogHandler
{
public:
    static GuardDogHandler& getInstance()
    {
        static GuardDogHandler instance;
        return instance;
    }

    std::shared_ptr<GuardDog> createGuardDog(unsigned int timeout_seconds)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto guarddog = std::make_shared<GuardDog>(*this, timeout_seconds);
        guarddogs_.push_back(guarddog);
        return guarddog;
    }

    // Feed the hardware watchdog
    void feedHardwareWatchdog()
    {
        if (watchdog_stream_.is_open())
        {
            watchdog_stream_ << "V";  // Write to feed the watchdog
            watchdog_stream_.flush(); // Ensure the data is written immediately
        }
    }

private:
    GuardDogHandler()
    {
        // Open the /dev/watchdog device to initialize feeding
        watchdog_stream_.open("/dev/watchdog");
        if (!watchdog_stream_.is_open())
        {
            std::cerr << "Failed to open /dev/watchdog. Is the watchdog module enabled?" << std::endl;
            exit(1);  // Exit if the watchdog cannot be opened
        }
        std::cout << "Hardware GuardDogHandler initialized.\n";
    }

    // Prevent copying
    GuardDogHandler(const GuardDogHandler&) = delete;
    GuardDogHandler& operator=(const GuardDogHandler&) = delete;

    std::vector<std::shared_ptr<GuardDog>> guarddogs_;
    std::mutex mutex_;
    std::ofstream watchdog_stream_;  // Stream to interact with /dev/watchdog
};

/*
int main()
{
    // Create the GuardDogHandler (singleton)
    auto& handler = GuardDogHandler::getInstance();

    // Create a GuardDog with a 5-second timeout
    auto guarddog1 = handler.createGuardDog(5);

    // Simulate feeding the GuardDog periodically
    std::thread feedingThread([&guarddog1]() {
        for (int i = 0; i < 10; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            guarddog1->feed();  // Reset timer each time
            std::cout << "Fed the GuardDog.\n";
        }
    });

    // Simulate stopping the GuardDog and later reactivating it
    std::this_thread::sleep_for(std::chrono::seconds(10));
    guarddog1->sitBoy();  // Deactivate
    std::cout << "GuardDog deactivated.\n";

    // Reactivate after some time
    std::this_thread::sleep_for(std::chrono::seconds(5));
    guarddog1->reactivate();  // Reactivate the GuardDog
    std::cout << "GuardDog reactivated.\n";

    // Join the feeding thread to ensure it runs to completion
    feedingThread.join();

    return 0;
}
*/