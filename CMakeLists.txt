cmake_minimum_required(VERSION 3.28)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API ON)
project(RealTimeRaytracer LANGUAGES CXX)

# ----------------------------------------------------------------------------
# C++ Standard
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# Vulkan
# ----------------------------------------------------------------------------

# pick a version and root for your unpacked SDK
set(VULKAN_SDK_VERSION "1.4.309.0")
set(VULKAN_SDK_ROOT   "$ENV{HOME}/tools/vulkan/${VULKAN_SDK_VERSION}/x86_64")

# Try system Vulkan
find_package(Vulkan QUIET)

# Fallback to your local SDK if system failed
if(NOT Vulkan_FOUND)
    if(EXISTS "${VULKAN_SDK_ROOT}")
        message(STATUS "Using Vulkan SDK at ${VULKAN_SDK_ROOT}")
        # let FindVulkan know where the SDK lives
        set(ENV{VULKAN_SDK} "${VULKAN_SDK_ROOT}")
        # Ensure CMAKE_FIND_* paths include your SDK’s include
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
# Convert GLSL files to SPIR-V
# ----------------------------------------------------------------------------

# bind glslc program
# Try to find glslc on the system
find_program(SYSTEM_GLSLC glslc)

# Fallback to Vulkan SDK path if not found
if(NOT SYSTEM_GLSLC)
    set(GLSLC_EXECUTABLE "${VULKAN_SDK_ROOT}/bin/glslc")
else()
    set(GLSLC_EXECUTABLE "${SYSTEM_GLSLC}")
endif()

# Cache the path so it shows up in CMake GUIs and is reused
set(GLSLC_EXECUTABLE "${GLSLC_EXECUTABLE}" CACHE STRING "Path to glslc executable")

message(STATUS "Using glslc: ${GLSLC_EXECUTABLE}")

# Set shader source and build paths
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
set(SHADER_BIN_DIR ${CMAKE_BINARY_DIR}/shaders/spv)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

file(GLOB_RECURSE GLSL_SHADERS
        ${SHADER_SRC_DIR}/*.rgen
        ${SHADER_SRC_DIR}/*.rchit
        ${SHADER_SRC_DIR}/*.rmiss
        ${SHADER_SRC_DIR}/*.rint
)

message(STATUS "Found shaders:")
foreach(s IN LISTS GLSL_SHADERS)
    message(STATUS "  ${s}")
endforeach()

set(SPV_TARGETS "")

foreach(SHADER ${GLSL_SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    get_filename_component(EXT ${SHADER} EXT)

    set(SPV ${SHADER_BIN_DIR}/${FILE_NAME}.spv)

    add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${FILE_NAME}"
            COMMAND ${GLSLC_EXECUTABLE}
            --target-env=vulkan1.2
            -o ${SPV} ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${FILE_NAME} → ${SPV}"
            VERBATIM
    )

    list(APPEND SPV_TARGETS ${SPV})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPV_TARGETS})


# ----------------------------------------------------------------------------
# Main executable
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/app/window.cppm
        src/vulkan/memory/descriptor_set_layout.cppm
        src/vulkan/memory/descriptor_pool.cppm
        src/scene/camera.cppm
        src/vulkan/context/command_pool.cppm
        src/scene/geometry/triangle.cppm
        src/scene/geometry/vertex.cppm
        src/vulkan/memory/image.cppm
        src/vulkan/raytracing/blas.cppm
        src/vulkan/raytracing/tlas.cppm
        external/stb/stb_image.h
        src/app/setup/geometry_builder.cppm
        src/vulkan/memory/image_sampler.cppm
        src/scene/geometry/object.cppm
        src/scene/textures/texture.cppm
)

# --- MODULE SUPPORT -----------------------------------------
file(GLOB_RECURSE MODULE_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
)

# Tell CMake that these files are CXX_MODULES for our target
target_sources(${PROJECT_NAME}
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${MODULE_SOURCES}
)

# include Vulkan headers if we fetched them
if(TARGET Vulkan::Vulkan AND NOT Vulkan_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${vulkan-headers_SOURCE_DIR}/include
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external/stb)
# Add tinyobjloader subdirectory
add_subdirectory(external/tinyobjloader)

# Link against the loader, GLFW, and GLM
target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
        tinyobjloader
)

target_compile_definitions(RealTimeRaytracer
        PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1   # tell every TU to use the dynamic dispatcher
)

add_dependencies(${PROJECT_NAME} CompileShaders)