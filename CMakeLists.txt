cmake_minimum_required(VERSION 3.26)
project(RealTimeRaytracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ────────────────────────────────────────────────────────────────
# 1) Third‑party base dir
# ────────────────────────────────────────────────────────────────
set(THIRD_PARTY_DIR "$ENV{HOME}/tools"
        CACHE PATH "Where to put downloaded/external dependencies")
# ensure subfolders exist
file(MAKE_DIRECTORY
        "${THIRD_PARTY_DIR}/vulkan"
        "${THIRD_PARTY_DIR}/glfw"
)

# ────────────────────────────────────────────────────────────────
# 2) Vulkan SDK (LunarG, tar.xz)
# ────────────────────────────────────────────────────────────────
set(VULKAN_SDK_VERSION   "1.4.309.0" CACHE STRING "Vulkan SDK version")
set(VULKAN_TARBALL       "vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz")
set(VULKAN_DOWNLOAD_URL  "https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/${VULKAN_TARBALL}")
set(VULKAN_SDK_ROOT      "${THIRD_PARTY_DIR}/vulkan/${VULKAN_SDK_VERSION}")

if(NOT EXISTS "${VULKAN_SDK_ROOT}")
    message(STATUS "Downloading Vulkan SDK ${VULKAN_SDK_VERSION}…")
    file(DOWNLOAD
            "${VULKAN_DOWNLOAD_URL}"
            "${THIRD_PARTY_DIR}/vulkan/${VULKAN_TARBALL}"
            SHOW_PROGRESS STATUS _dlstat
    )
    list(GET _dlstat 0 _code)
    if(NOT _code EQUAL 0)
        message(FATAL_ERROR "Failed to download Vulkan SDK:\n$<JOIN:${_dlstat},\n>")
    endif()

    message(STATUS "Extracting Vulkan SDK…")
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xJf "${THIRD_PARTY_DIR}/vulkan/${VULKAN_TARBALL}"
            WORKING_DIRECTORY "${THIRD_PARTY_DIR}/vulkan"
    )
    file(REMOVE "${THIRD_PARTY_DIR}/vulkan/${VULKAN_TARBALL}")
endif()

# point CMake to the unpacked SDK
set(ENV{VULKAN_SDK}     "${VULKAN_SDK_ROOT}/x86_64")
list(APPEND CMAKE_PREFIX_PATH "${VULKAN_SDK_ROOT}/x86_64")

find_package(Vulkan REQUIRED)
message(STATUS "Using Vulkan from ${Vulkan_INCLUDE_DIRS}")

# ────────────────────────────────────────────────────────────────
# 3) GLFW (via FetchContent if needed)
# ────────────────────────────────────────────────────────────────
find_package(glfw3 3.3 QUIET)

if (NOT glfw3_FOUND)
    # fetch the source
    FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.3.8         # or whatever version you need
    )

    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(glfw)
endif()

# ────────────────────────────────────────────────────────────────
# 4) GLM (via FetchContent if needed)
# ────────────────────────────────────────────────────────────────
find_package(glm 1.0.1 QUIET)
if(NOT glm_FOUND)
    FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG        1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# ────────────────────────────────────────────────────────────────
# 5) Your executable
# ────────────────────────────────────────────────────────────────
add_executable(RealTimeRaytracer src/main.cpp)
target_link_libraries(RealTimeRaytracer PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)
