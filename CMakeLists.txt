cmake_minimum_required(VERSION 3.10)

project(RaspPi_LED_Controller)

# Specify where the build files should be output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
message(STATUS "STATUS: Output Archive Directory: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
message(STATUS "STATUS: Output Library Directory: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin")
message(STATUS "STATUS: Output Runtime Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Find ALSA
find_package(ALSA REQUIRED)

# Include KissFFT source files directly
file(GLOB KISSFFT_SRC
    ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.c
    ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.h
)

# Collect all .cpp and .h files in the src directory and its subdirectories
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "submodules/spdlog/include/spdlog.h"
)

# Exclude specific unwanted files
list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/bench/.*")
list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/tests/.*")

# Output the list of files collected
message(STATUS "STATUS: Collected source files: ${SOURCES}")

# Define the executable
add_executable(RaspPi_LED_Controller ${SOURCES} ${KISSFFT_SRC})

# Include necessary directories
include_directories(
    ${CMAKE_SOURCE_DIR}/submodules/spdlog/include
    ${CMAKE_SOURCE_DIR}/submodules/kissfft
)

# Link ALSA (no need to find KissFFT separately now)
target_link_libraries(RaspPi_LED_Controller PRIVATE asound)
