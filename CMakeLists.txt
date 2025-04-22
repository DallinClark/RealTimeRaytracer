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
# Vulkan
# ----------------------------------------------------------------------------
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    message(STATUS "Vulkan not found—fetching Vulkan-Headers & Vulkan-Loader")
    # Headers
    FetchContent_Declare(
            Vulkan-Headers
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
            GIT_TAG        vulkan-sdk-1.4.309.0
    )
    # Loader
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            Vulkan-Loader
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
            GIT_TAG        vulkan-sdk-1.4.309.0
    )
    FetchContent_MakeAvailable(Vulkan-Headers Vulkan-Loader)

    # Expose a Vulkan::Vulkan target
    add_library(vulkan STATIC
            $<TARGET_OBJECTS:Vulkan-Loader>
    )
    target_include_directories(vulkan PUBLIC
            ${vulkan-headers_SOURCE_DIR}/include
    )
    add_library(Vulkan::Vulkan ALIAS vulkan)
else()
    message(STATUS "Using system Vulkan")
endif()

# ----------------------------------------------------------------------------
# GLFW
# ----------------------------------------------------------------------------
find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw)
    message(STATUS "GLFW not found—fetching glfw")
    FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.3.10
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
else()
    message(STATUS "Using system GLFW")
endif()

# ----------------------------------------------------------------------------
# GLM
# ----------------------------------------------------------------------------
find_package(glm CONFIG QUIET)
if(NOT TARGET glm::glm)
    message(STATUS "GLM not found—fetching glm")
    FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG        1.0.1
    )
    FetchContent_MakeAvailable(glm)
else()
    message(STATUS "Using system GLM")
endif()

# ----------------------------------------------------------------------------
# Main executable
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
        src/main.cpp
)

# include Vulkan headers if we fetched them
if(TARGET Vulkan::Vulkan AND NOT Vulkan_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${vulkan-headers_SOURCE_DIR}/include
    )
endif()

# Link against the loader, GLFW, and GLM
target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)
