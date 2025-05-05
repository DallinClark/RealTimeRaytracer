#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <print>

import WindowManager;
import VulkanContext;

int main() {
    app::WindowManager wm{"RealTimeRayTracer", 800, 600};
    GLFWwindow* window = wm.getWindow();

    // vulkan::VulkanContext vkContext{"RealTimeRaytracer", window};

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::println("{} extensions supported", extensionCount);

    glm::mat4 matrix;
    glm::vec4 vec;
    auto test = matrix * vec;

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    return 0;
}