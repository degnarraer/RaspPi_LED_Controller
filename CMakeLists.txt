cmake_minimum_required(VERSION 3.10)

project(RaspPi_LED_Controller)

# Specify where the build files should be output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
message(STATUS "STATUS: Output Archive Directory: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
message(STATUS "STATUS: Output Library Directory: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin")
message(STATUS "STATUS: Output Runtime Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Step 1: Install the required packages (excluding npm due to conflict)
    set(PACKAGES 
        libasound2-dev
        nlohmann-json3-dev
        cmake
        libboost-all-dev
        nodejs  # Installed via NodeSource, but still included in case it's valid
        nginx
        rpi-ws281x-cpp 
    )

    # Skip broken apt npm installation
    # npm will be checked manually below
    foreach(PKG IN LISTS PACKAGES)
        execute_process(
            COMMAND dpkg -s ${PKG}
            RESULT_VARIABLE PKG_CHECK_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(PKG_CHECK_RESULT EQUAL 0)
            message(STATUS "${PKG} is already installed.")
        else()
            message(STATUS "Installing ${PKG}...")
            execute_process(
                COMMAND sudo apt-get install -y ${PKG}
                RESULT_VARIABLE INSTALL_RESULT
            )

            if(NOT INSTALL_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to install ${PKG}")
            else()
                message(STATUS "${PKG} installed successfully.")
            endif()
        endif()
    endforeach()

# Step 2: Check for npm manually
    execute_process(
        COMMAND npm --version
        RESULT_VARIABLE NPM_RESULT
        OUTPUT_VARIABLE NPM_VERSION
        ERROR_VARIABLE NPM_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT NPM_RESULT EQUAL 0)
        message(FATAL_ERROR "npm is not installed or not functioning correctly. Please install it using the NodeSource setup script.")
    else()
        message(STATUS "npm is installed: ${NPM_VERSION}")
    endif()

# Step 3: Check for node manually
    execute_process(
        COMMAND node --version
        RESULT_VARIABLE NODE_RESULT
        OUTPUT_VARIABLE NODE_VERSION
        ERROR_VARIABLE NODE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT NODE_RESULT EQUAL 0)
        message(FATAL_ERROR "node is not installed or not functioning correctly. Please install it using the NodeSource setup script.")
    else()
        message(STATUS "node is installed: ${NODE_VERSION}")
    endif()

# Step 4 Include KissFFT source files directly
    file(GLOB KISSFFT_SRC
        ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.c
        ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.h
    )

# Step 5 Collect all .cpp and .h files in the src directory and its subdirectories
    file(GLOB_RECURSE SOURCES 
        "back_end/*.cpp"
        "back_end/*.h"
        "submodules/spdlog/include/spdlog.h"
    )

    # Exclude specific unwanted files
    list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/bench/.*")
    list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/tests/.*")

    # Output the list of files collected
    message(STATUS "STATUS: Collected source files:")
    foreach(SOURCE_FILE ${SOURCES})
        message(STATUS "  ${SOURCE_FILE}")
    endforeach()

# Step 6 Define the executable
add_executable(RaspPi_LED_Controller ${SOURCES} ${KISSFFT_SRC})

# Step 7 Include necessary directories
include_directories(
    ${CMAKE_SOURCE_DIR}/submodules/spdlog/include
    ${CMAKE_SOURCE_DIR}/submodules/kissfft
)

# Step 6 Link ALSA (no need to find KissFFT separately now)
target_link_libraries(RaspPi_LED_Controller PRIVATE asound)