module;

#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string_view>

export module app;

import vulkan.context.instance;
import vulkan.context.device;
import vulkan.context.surface;
import vulkan.context.swapchain;
import vulkan.dispatch;

export namespace app {

class Application {
public:
    Application(std::string_view title = "Vulkan RayTracer",
                uint32_t width         = 800,
                uint32_t height        = 600,
                bool     enableValidation = true)
    {
        vulkan::dispatch::init_loader();

        // ---- GLFW window setup ----
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.data(), nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // ---- Vulkan context construction ----
        instance_  = std::make_unique<vulkan::context::Instance>(title, enableValidation);
        surface_   = std::make_unique<vulkan::context::Surface>(*instance_, window_);
        device_    = std::make_unique<vulkan::context::Device>(*instance_, surface_->get(), enableValidation);
        swapchain_ = std::make_unique<vulkan::context::Swapchain>(*device_, *surface_);
    }

    ~Application() {
        // Destroy in reverse order of creation
        swapchain_.reset();
        device_.reset();
        surface_.reset();
        instance_.reset();

        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
    }

    /// Simple event loop
    void run() const {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            // ← in the future: record & submit ray-tracing commands here
        }
    }

private:
    GLFWwindow* window_{nullptr};

    std::unique_ptr<vulkan::context::Instance>  instance_;
    std::unique_ptr<vulkan::context::Surface>   surface_;
    std::unique_ptr<vulkan::context::Device>    device_;
    std::unique_ptr<vulkan::context::Swapchain> swapchain_;
};

} // namespace app
