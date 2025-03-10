cmake_minimum_required(VERSION 3.10)

# Specify where the build files should be output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/lib")
message(STATUS "Output Archive Directory: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/lib")
message(STATUS "Output Library Directory: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin")
message(STATUS "Output Runtime Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

project(RaspPi_LED_Controller)

# Find ALSA
find_package(ALSA REQUIRED)

# Collect all .cpp and .h files in the src directory and its subdirectories
file(GLOB_RECURSE SOURCES "**/*.cpp" "**/*.h")

# Output the list of files collected
message(STATUS "Collected source files: ${SOURCES}")

# Define the executable
add_executable(RaspPi_LED_Controller ${SOURCES})

# Link ALSA
target_link_libraries(RaspPi_LED_Controller PRIVATE asound)
