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

# pick a version and root for your unpacked SDK
set(VULKAN_SDK_VERSION "1.4.309.0")
set(VULKAN_SDK_ROOT   "$ENV{HOME}/tools/vulkan/${VULKAN_SDK_VERSION}/x86_64")

# 1) Try system Vulkan
find_package(Vulkan QUIET)

# 2) Fallback to your local SDK if system failed
if(NOT Vulkan_FOUND)
    if(EXISTS "${VULKAN_SDK_ROOT}")
        message(STATUS "Using Vulkan SDK at ${VULKAN_SDK_ROOT}")
        # let FindVulkan know where the SDK lives (added in CMake 3.18)&#8203;:contentReference[oaicite:2]{index=2}
        set(ENV{VULKAN_SDK} "${VULKAN_SDK_ROOT}")
        # Ensure CMAKE_FIND_* paths include your SDK’s include/& lib directories&#8203;:contentReference[oaicite:3]{index=3}
        list(APPEND CMAKE_PREFIX_PATH "${VULKAN_SDK_ROOT}")
        # And re‑invoke find_package so it actually picks up headers & loader from there
        find_package(Vulkan REQUIRED)
    else()
        message(FATAL_ERROR
                "Vulkan not found: neither system Vulkan nor SDK at\n  ${VULKAN_SDK_ROOT}")
    endif()
endif()

# At this point: Vulkan_FOUND is true and
# Vulkan_INCLUDE_DIRS / Vulkan_LIBRARIES are set correctly.
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries:     ${Vulkan_LIBRARIES}")

# ----------------------------------------------------------------------------
# GLFW
# ----------------------------------------------------------------------------

# First try system install
find_package(glfw3 CONFIG QUIET)
if (TARGET glfw)
    message(STATUS "Using system-installed GLFW")
else()
    # Try ~/tools/glfw next
    find_package(glfw3 CONFIG QUIET
            PATHS "$ENV{HOME}/tools/glfw"
            NO_DEFAULT_PATH
    )
    if (TARGET glfw)
        message(STATUS "Using GLFW from $ENV{HOME}/tools/glfw")
    else()
        message(FATAL_ERROR
                "Could not find GLFW:\n"
                "  - not found as a system-installed package\n"
                "  - not found in $ENV{HOME}/tools/glfw\n"
                "Please install GLFW or adjust CMAKE_PREFIX_PATH."
        )
    endif()
endif()



# ----------------------------------------------------------------------------
# GLM
# ----------------------------------------------------------------------------

# First try system install
find_package(glm CONFIG QUIET)

# If that fails, look in ~/tools/lm
if (NOT TARGET glm::glm)
    message(STATUS "System GLM not found; checking ~/tools/glm")
    find_package(glm CONFIG REQUIRED
            PATHS "$ENV{HOME}/tools/glm"
            NO_DEFAULT_PATH
    )
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
