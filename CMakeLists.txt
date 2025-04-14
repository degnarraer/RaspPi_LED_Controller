cmake_minimum_required(VERSION 3.10)
project(RaspPi_LED_Controller)

############ Setup Build Output Folders ############
    message(STATUS "STEP: Setting up Build Output Folders")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
    message(STATUS "  Output Archive Directory: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
    message(STATUS "  Output Library Directory: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin")
    message(STATUS "  Output Runtime Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

############ Install Required Packages ############
    message(STATUS "STEP: Installing required Packages")
    set(PACKAGES 
        libasound2-dev
        nlohmann-json3-dev
        cmake
        libboost-all-dev
        nodejs
        nginx
        python3-pip
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
            message(STATUS "  ${PKG} is already installed.")
        else()
            message(STATUS "  Installing ${PKG}...")
            execute_process(
                COMMAND sudo apt-get install -y ${PKG}
                RESULT_VARIABLE INSTALL_RESULT
            )

            if(NOT INSTALL_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to install ${PKG}")
            else()
                message(STATUS "  ${PKG} installed successfully.")
            endif()
        endif()
    endforeach()

############ Check for Required Things ############
    message(STATUS "STEP: Checking for required things")
    # Look for the ALSA library
    find_library(ALSA_LIBRARY NAMES asound)

    if(NOT ALSA_LIBRARY)
        message(FATAL_ERROR "ALSA (asound) library not found.")
    else()
        message(STATUS "  ALSA library found at ${ALSA_LIBRARY}")
    endif()

    # Find Boost libraries (including Boost.Python)
    find_package(Boost REQUIRED COMPONENTS Python)

    if(Boost_FOUND)
        message(STATUS "  Boost.Python found!")
        message(STATUS "  Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
        message(STATUS "  Boost_LIBRARIES: ${Boost_LIBRARIES}")
    else()
        message(FATAL_ERROR "Boost.Python not found. Please install libboost-python3-dev.")
    endif()

############ Manually Setup NPM ############
    message(STATUS "STEP: Setting up NPM")
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
        message(STATUS "  npm is installed: ${NPM_VERSION}")
    endif()


############ Manually Setup Node ############
    message(STATUS "STEP: Setting up Node")
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
        message(STATUS "  node is installed: ${NODE_VERSION}")
    endif()

############ Add WS2812 Library ############
    add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/rpi_ws281x)

############ Get all cpp and h files ############
    message(STATUS "STEP: Getting all included files")
    file(GLOB KISSFFT_SRC
        ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.c
        ${CMAKE_SOURCE_DIR}/submodules/kissfft/*.h
    )
    file(GLOB_RECURSE SOURCES 
        "back_end/*.cpp"
        "back_end/*.h"
        "submodules/spdlog/include/spdlog.h"
    )

    # Exclude specific unwanted files
    list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/bench/.*")
    list(FILTER SOURCES EXCLUDE REGEX "submodules/spdlog/tests/.*")

    # Output the list of files collected
    message(STATUS "  Collected source files:")
    foreach(SOURCE_FILE ${SOURCES})
        message(STATUS "    ${SOURCE_FILE}")
    endforeach()

    message(STATUS "STEP: Defining Executable")
    add_executable(RaspPi_LED_Controller ${SOURCES} ${KISSFFT_SRC})

    # Include necessary directories
    include_directories(
        ${CMAKE_SOURCE_DIR}/submodules/rpi_ws281x/include
        ${CMAKE_SOURCE_DIR}/submodules/spdlog/include
        ${CMAKE_SOURCE_DIR}/submodules/kissfft
    )

############ Link Libraries ############
    message(STATUS "STEP: Linking")
    target_link_libraries(RaspPi_LED_Controller PRIVATE asound
        ws2811
    )


