cmake_minimum_required(VERSION 3.26)
project(RealTimeRaytracer LANGUAGES CXX)

# ----------------------------------------------------------------------------
# C++ Standard
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# FetchContent configuration: put all sources/builds inside the build tree
# ----------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps"
        CACHE PATH "Base directory for FetchContent downloads and builds")

# ----------------------------------------------------------------------------
# Vulkan-Headers
# ----------------------------------------------------------------------------
FetchContent_Declare(
        Vulkan-Headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        vulkan-sdk-1.4.309.0
)
FetchContent_MakeAvailable(Vulkan-Headers)

# ----------------------------------------------------------------------------
# Vulkan-Loader
# ----------------------------------------------------------------------------
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        Vulkan-Loader
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
        GIT_TAG        vulkan-sdk-1.4.309.0
)
FetchContent_MakeAvailable(Vulkan-Loader)

add_library(Vulkan::Vulkan ALIAS vulkan)

# ----------------------------------------------------------------------------
# GLFW
# ----------------------------------------------------------------------------
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.10
)
FetchContent_MakeAvailable(glfw)

# ----------------------------------------------------------------------------
# GLM
# ----------------------------------------------------------------------------
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# ----------------------------------------------------------------------------
# Main executable
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
        src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${vulkan-headers_SOURCE_DIR}/include
)

# Link against the loader, GLFW, and GLM
target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)
