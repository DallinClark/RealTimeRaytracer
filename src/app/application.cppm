module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

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
import vulkan.context.command_pool;
import vulkan.dispatch;
import vulkan.memory.descriptor_pool;
import vulkan.memory.descriptor_set_layout;
import vulkan.memory.buffer;
import vulkan.ray_tracing_pipeline;
import vulkan.memory.image;
import vulkan.raytracing.blas;


import scene.camera;
import scene.geometry.vertex;
import scene.geometry.triangle;
#include <vulkan/vulkan_enums.hpp>

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

        vulkan::context::CommandPool commandPool(device_->get(), device_->computeFamilyIndex());

        vk::Extent3D extent = { swapchain_->extent().width, swapchain_->extent().height, 1 };

        vulkan::memory::Image storageImage(
                device_->get(),
                device_->physical(),
                extent,
                vk::Format::eR32G32B32A32Sfloat,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageAspectFlagBits::eColor
        );



        glm::vec3 cameraPosition{ 0.0f,0.0f,1.0f };
        glm::vec3   cameraLookAt{ 0.0f,0.0f,0.0f };
        glm::vec3       cameraUp{ 0.0f,1.0f,0.0f };
        float fovY = 90;
        int pixelWidth  = 800;
        int pixelHeight = 600;

        scene::Camera camera(*device_, fovY, cameraPosition, cameraLookAt, cameraUp, pixelWidth, pixelHeight);
        vk::Buffer cameraBuffer = camera.getBuffer();

        std::vector<glm::vec3> triangleVertices = {
                { -0.5f, -0.5f, 0.0f },
                {  0.5f, -0.5f, 0.0f },
                {  0.0f,  0.5f, 0.0f }
        };

        vulkan::memory::Buffer vertexBuffer(
                *device_,
                sizeof(glm::vec3) * triangleVertices.size(),
                vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        vertexBuffer.fill(triangleVertices.data(), sizeof(glm::vec3) * triangleVertices.size(), 0);

        vulkan::raytracing::BLAS triangleBLAS (*device_, commandPool, vertexBuffer.get(), 3, sizeof(glm::vec3));


        /* JUST FOR TESTING */
        // FOR NOW JUST ONE DESCRIPTOR SET
        vulkan::memory::DescriptorSetLayout layout(device_->get());
        // Create the camera

        layout.addBinding(0, vk::DescriptorType::eStorageImage,  vk::ShaderStageFlagBits::eRaygenKHR); // image
        layout.addBinding(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // camera
        layout.addBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR); // vertex buffer
        layout.build();

        // We need this to happen automatically based off of needs
        std::vector<vk::DescriptorPoolSize> poolSizes = {
                {vk::DescriptorType::eStorageImage, 1},
                {vk::DescriptorType::eStorageBuffer, 1},
                {vk::DescriptorType::eUniformBuffer, 1}
        };

        vulkan::memory::DescriptorPool pool(device_->get(), poolSizes, 3);
        vk::DescriptorSet set = pool.allocate(layout)[0];

        pool.writeImage(set, 0, storageImage.getImageInfo(), vk::DescriptorType::eStorageImage);
        pool.writeBuffer(set, 2, cameraBuffer, sizeof(scene::Camera::GPUCameraData), vk::DescriptorType::eUniformBuffer);
        pool.writeBuffer(set, 3, vertexBuffer.get(), sizeof(glm::vec3) * triangleVertices.size() , vk::DescriptorType::eStorageBuffer);

        std::vector<vk::DescriptorSetLayout> descriptorLayouts{layout.get()};

        auto raytracePipeline = vulkan::RayTracingPipeline(*device_, descriptorLayouts, "shaders/spv/raygen.rgen.spv", "shaders/spv/miss.rmiss.spv", "shaders/spv/closesthit.rchit.spv");

        while (!glfwWindowShouldClose(window_)) {
            auto cmd = commandPool.getSingleUseBuffer();
            //cmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracingPipelineLayout_, 0, sets, {});
            //commandPool.submitSingleUse(std::move(cmd), device_->getGraphicsQueue());

            glfwPollEvents();
            // in the future: record & submit ray-tracing commands here
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
