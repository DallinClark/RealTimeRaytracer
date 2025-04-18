cmake_minimum_required(VERSION 3.30)
project(RealTimeRaytracer LANGUAGES CXX)

# -- C++ Standard and Modules ----------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -- Dependencies ----------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm 0.9.9 REQUIRED)

# -- Source Layout ---------------------------------------------------------
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# -- Main Executable -------------------------------------------------------
add_executable(RealTimeRaytracer
        ${SRC_DIR}/main.cpp
)
target_compile_features(RealTimeRaytracer PRIVATE cxx_std_23)
target_link_libraries(RealTimeRaytracer
        PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)
