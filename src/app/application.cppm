module;

#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string_view>

export module app;

import core.log;
import vulkan.context.instance;
import vulkan.context.device;
import vulkan.context.surface;
import vulkan.context.swapchain;
import vulkan.dispatch;

export namespace app {

class Application {
public:
    explicit Application(
            std::string_view title = "Real Time RayTracer",
            const uint32_t width         = 800,
            const uint32_t height        = 600,
            bool     enableValidation = true
    ) {
        core::log::info("Starting application '{}'", title);
        vulkan::dispatch::init_loader();

        // ---- GLFW window setup ----
        if (!glfwInit()) {
            core::log::error("Failed to initialise GLFW");
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.data(), nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            core::log::error("Failed to create GLFW window");
            throw std::runtime_error("Failed to create GLFW window");
        }
        core::log::info("Window created ({}×{})", width, height);

        // ---- Vulkan context construction ----
        instance_  = std::make_unique<vulkan::context::Instance>(title, enableValidation);
        core::log::debug("Vulkan instance ready");
        surface_   = std::make_unique<vulkan::context::Surface>(*instance_, window_);
        core::log::debug("Surface created");
        device_    = std::make_unique<vulkan::context::Device>(*instance_, surface_->get(), enableValidation);
        core::log::debug("Logical device initialised");
        swapchain_ = std::make_unique<vulkan::context::Swapchain>(*device_, *surface_);
        core::log::debug("Swap-chain set up");
    }

    ~Application() {
        core::log::info("Shutting down application");

        // Destroy in reverse order of creation
        swapchain_.reset();
        device_.reset();
        surface_.reset();
        instance_.reset();

        if (window_) {
            glfwDestroyWindow(window_);
            core::log::debug("Window destroyed");
        }
        glfwTerminate();
        core::log::debug("GLFW terminated");
    }

    /// Simple event loop
    void run() const {
        core::log::info("Entering main loop");
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            // ← in the future: record & submit ray-tracing commands here
        }
        core::log::info("Main loop exited");
    }

private:
    GLFWwindow* window_{nullptr};

    std::unique_ptr<vulkan::context::Instance>  instance_;
    std::unique_ptr<vulkan::context::Surface>   surface_;
    std::unique_ptr<vulkan::context::Device>    device_;
    std::unique_ptr<vulkan::context::Swapchain> swapchain_;
};

} // namespace app
