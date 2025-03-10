cmake_minimum_required(VERSION 3.10)
project(MyRpiProject)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

# Add the ALSA library directory
link_directories(/usr/lib/aarch64-linux-gnu)

# Specify the executable
add_executable(main_new main.cpp)


find_package(ALSA REQUIRED)

# Link with the ALSA library
target_link_libraries(main asound)
