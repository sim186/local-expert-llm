cmake_minimum_required(VERSION 3.20)
project(LocalLLM VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Add llama.cpp library
# Note: llama.cpp will be added as a git submodule in external/llama.cpp
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "Disable llama.cpp tests")
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "Disable llama.cpp examples")
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "Disable llama.cpp server")

# Check if llama.cpp submodule exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/CMakeLists.txt")
    add_subdirectory(external/llama.cpp)
else()
    message(WARNING "llama.cpp submodule not found. Run: git submodule update --init --recursive")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/llamaworker.cpp
)

set(HEADERS
    src/mainwindow.h
    src/llamaworker.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
)

# Link llama.cpp library if available
if(TARGET llama)
    target_link_libraries(${PROJECT_NAME} PRIVATE llama)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/common
    )
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Platform-specific settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

if(WIN32)
    # Ensure console window shows for debugging output
    # Set to ON for release builds without console
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE OFF
    )
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "LocalLLM Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version:   ${Qt6_VERSION}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "")
