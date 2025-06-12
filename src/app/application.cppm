module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

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
import vulkan.raytracing.tlas;
import app.window;

import core.file;

import scene.camera;
import scene.geometry.vertex;
import scene.geometry.triangle;

import app.setup.geometry_builder;

import vulkan.memory.image_sampler;

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

        window_ = std::make_unique<app::Window>("Realtime Raytracer", width, height, MOUSE_SENSITIVITY);
        core::log::info("Window created ({}×{})", width, height);

        // ---- Vulkan context construction ----
        instance_  = std::make_unique<vulkan::context::Instance>(title, enableValidation);
        core::log::debug("Vulkan instance ready");
        surface_   = std::make_unique<vulkan::context::Surface>(*instance_, window_->get());
        core::log::debug("Surface created");
        device_    = std::make_unique<vulkan::context::Device>(*instance_, surface_->get(), enableValidation);
        core::log::debug("Logical device initialised");
        swapchain_ = std::make_unique<vulkan::context::Swapchain>(*device_, *surface_);
        core::log::debug("Swap-chain set up");

        glm::vec3 initialCameraPosition{ 0.0f,0.0f,5.0f };
        glm::vec3   initialCameraLookAt{ 0.0f,0.0f,0.0f };
        glm::vec3              cameraUp{ 0.0f,1.0f,0.0f };
        float fovY = 60;
        int pixelWidth  = 1600;
        int pixelHeight = 1200;

        camera_ = std::make_unique<scene::Camera> (*device_, fovY, initialCameraPosition, initialCameraLookAt, cameraUp, pixelWidth, pixelHeight);
        core::log::debug("Camera created");

        window_->setCamera(camera_.get());
    }

    ~Application() {
        core::log::info("Shutting down application");

        // Destroy in reverse order of creation
        camera_.reset();
        swapchain_.reset();
        device_.reset();
        surface_.reset();
        instance_.reset();
    }

    /// Simple event loop
    void run() const {
        core::log::info("Entering main loop");

        vulkan::context::CommandPool commandPool(device_->get(), device_->computeFamilyIndex());

        vk::Extent3D extent = { swapchain_->extent().width, swapchain_->extent().height, 1 };

        vulkan::memory::Image outputImage(device_->get(), device_->physical(), extent,
                vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

        auto cmdImage = commandPool.getSingleUseBuffer(); // TODO maybe wrap this a bit better
        vulkan::memory::Image::setImageLayout(*cmdImage, outputImage.getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
        commandPool.submitSingleUse(std::move(cmdImage), device_->computeQueue());
        device_->get().waitIdle();

        auto textureImage = core::file::createTextureImage(*device_.get(), "../../assets/textures/pig_head_tex.jpg", commandPool);
        vulkan::memory::ImageSampler texSampler(*device_.get());

        auto hdriImage = core::file::createTextureImage(*device_.get(), "../../assets/textures/pisztyk_4k.hdr", commandPool);
        vulkan::memory::ImageSampler hdriSampler(*device_.get());

        vk::Buffer cameraBuffer = camera_->getBuffer();

        std::vector<std::string> objPaths = { "../../assets/objects/pig_head.obj" , "../../assets/objects/bobo.obj"};
        std::vector<std::pair<vk::TransformMatrixKHR, int>> transforms = {
                {
                        vk::TransformMatrixKHR{
                                std::array<std::array<float, 4>, 3>{{
                                                                            {1.0f, 0.0f, 0.0f, 5.0f},  // Translate X by 5 units
                                                                            {0.0f, 1.0f, 0.0f, 0.0f},
                                                                            {0.0f, 0.0f, 1.0f, 0.0f}
                                                                    }}
                        },
                        0  // Index into objPaths (i.e., pig_head.obj)
                },
                {
                        vk::TransformMatrixKHR{
                                std::array<std::array<float, 4>, 3>{{
                                                                            {1.0f, 0.0f, 0.0f, 0.0f},  // Translate X by 0 units
                                                                            {0.0f, 1.0f, 0.0f, 0.0f},
                                                                            {0.0f, 0.0f, 1.0f, 0.0f}
                                                                    }}
                        },
                        0  // Index into objPaths (i.e., pig_head.obj)
                },
                {
                    vk::TransformMatrixKHR{
                            std::array<std::array<float, 4>, 3>{{
                                                                        {1.0f, 0.0f, 0.0f, -5.0f},  // Translate X by -5 units
                                                                        {0.0f, 1.0f, 0.0f, 0.0f},
                                                                        {0.0f, 0.0f, 1.0f, 0.0f}
                                                                }}
                    },
                            1  // Index into objPaths (i.e., pig_head.obj)
                }
        };
        auto geoReturnInfo = app::setup::GeometryBuilder::createTLASFromOBJsAndTransforms(*device_, commandPool, objPaths, transforms);
        std::vector<glm::vec3>                     vertexPositions = geoReturnInfo.vertexPositions;
        std::vector<scene::geometry::Vertex>       vertices = geoReturnInfo.vertices;
        std::vector<uint32_t>                      indices = geoReturnInfo.indices;
        std::unique_ptr<vulkan::raytracing::TLAS>  tlas = std::move(geoReturnInfo.tlas);

        // create shader vertex buffer
        vk::DeviceSize verticesSize = sizeof(scene::geometry::Vertex) * vertices.size();
        std::vector<vulkan::memory::Buffer::FillRegion> vertexDataRegions {
                {vertices.data() , verticesSize, 0},
        };
        vulkan::memory::Buffer vertexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, *device_, verticesSize,
                                              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                              | vk::BufferUsageFlagBits::eTransferDst, vertexDataRegions);

        vk::DeviceSize indicesSize = sizeof(uint32_t) * indices.size(); // TODO figure out how to just pass the objectBuffer with an offset
        std::vector<vulkan::memory::Buffer::FillRegion> indexDataRegions {
                {indices.data() , indicesSize, 0},
        };
        vulkan::memory::Buffer indexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, *device_, indicesSize,
                                                                                              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                                                              | vk::BufferUsageFlagBits::eTransferDst, indexDataRegions);


        /* JUST FOR TESTING */
        // FOR NOW JUST ONE DESCRIPTOR SET
        vulkan::memory::DescriptorSetLayout layout(device_->get());
        // Create the camera

        layout.addBinding(0, vk::DescriptorType::eStorageImage,  vk::ShaderStageFlagBits::eRaygenKHR); // image
        layout.addBinding(1, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // TLAS
        layout.addBinding(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // camera
        layout.addBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR); // vertex buffer
        layout.addBinding(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR);
        layout.addBinding(5, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eClosestHitKHR);
        layout.addBinding(6, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eMissKHR);

        layout.build();

        // We need this to happen automatically based off of needs
        std::vector<vk::DescriptorPoolSize> poolSizes = {
                {vk::DescriptorType::eStorageImage, 1},
                {vk::DescriptorType::eStorageBuffer, 2},
                {vk::DescriptorType::eUniformBuffer, 1},
                {vk::DescriptorType::eAccelerationStructureKHR, 1},
                {vk::DescriptorType::eCombinedImageSampler, 2}
        };

        vulkan::memory::DescriptorPool pool(device_->get(), poolSizes, 3);
        vk::DescriptorSet set = pool.allocate(layout)[0];

        static_assert(sizeof(scene::geometry::Vertex) == 48, "Vertex size must match shader layout");
        pool.writeImage(set, 0, outputImage.getImageInfo(), vk::DescriptorType::eStorageImage);
        pool.writeAccelerationStructure(set, 1, vk::DescriptorType::eAccelerationStructureKHR, tlas->get());
        pool.writeBuffer(set, 2, cameraBuffer, sizeof(scene::Camera::GPUCameraData), vk::DescriptorType::eUniformBuffer, 0);
        pool.writeBuffer(set, 3, vertexBuffer.get(), sizeof(scene::geometry::Vertex) * vertices.size() , vk::DescriptorType::eStorageBuffer, 0);
        pool.writeBuffer(set, 4, indexBuffer.get(), indices.size() * sizeof(uint32_t), vk::DescriptorType::eStorageBuffer, 0);
        pool.writeImage(set, 5, textureImage->getImageInfoWithSampler(texSampler.get()), vk::DescriptorType::eCombinedImageSampler);
        pool.writeImage(set, 6, hdriImage->getImageInfoWithSampler(hdriSampler.get()), vk::DescriptorType::eCombinedImageSampler);

        std::vector<vk::DescriptorSetLayout> descriptorLayouts{layout.get()};

        auto raytracePipeline = vulkan::RayTracingPipeline(*device_, descriptorLayouts, "shaders/spv/raygen.rgen.spv", "shaders/spv/miss.rmiss.spv", "shaders/spv/closesthit.rchit.spv");

        int frame = 0;
        float currentTime = window_->getTime();
        uint32_t imageIndex = 0;
        vk::UniqueSemaphore imageAcquiredSemaphore = device_->get().createSemaphoreUnique(vk::SemaphoreCreateInfo());

        // Main Loop
        while (!window_->shouldClose()) {
            float newTime = window_->getTime();
            float frameTime = newTime - currentTime;
            currentTime = newTime;

            camera_->updateGPUData();
            glfwPollEvents();
            window_->processInput(newTime, CAM_SPEED);

            imageIndex = swapchain_->acquireNextImage(imageAcquiredSemaphore);

            auto cmd = commandPool.getSingleUseBuffer();
            cmd->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.get());
            cmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.getLayout(), 0, set, nullptr);
            raytracePipeline.recordTraceRays(*cmd, extent);

            vk::Image srcImage = outputImage.getImage();
            vk::Image dstImage = swapchain_->getImage(imageIndex);
            vulkan::memory::Image::setImageLayout(*cmd, srcImage, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
            vulkan::memory::Image::setImageLayout(*cmd, dstImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            vulkan::memory::Image::copyImage(*cmd, srcImage, dstImage, extent);
            vulkan::memory::Image::setImageLayout(*cmd, srcImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
            vulkan::memory::Image::setImageLayout(*cmd, dstImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

            commandPool.submitSingleUse(std::move(cmd), device_->computeQueue());
            device_->get().waitIdle();

            vk::PresentInfoKHR presentInfo;
            vk::SwapchainKHR swapchain = swapchain_->get();
            std::vector<vk::SwapchainKHR> swapchains = { swapchain };
            presentInfo.setSwapchains(swapchains);
            presentInfo.setImageIndices(imageIndex);
            presentInfo.setWaitSemaphores(*imageAcquiredSemaphore);
            auto result = device_->presentQueue().presentKHR(presentInfo);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("failed to present.");
            }
            device_->presentQueue().waitIdle();
        }
        core::log::info("Main loop exited");
    }



private:
    std::unique_ptr<app::Window>   window_{nullptr};
    std::unique_ptr<scene::Camera> camera_{nullptr};

    std::unique_ptr<vulkan::context::Instance>  instance_;
    std::unique_ptr<vulkan::context::Surface>   surface_;
    std::unique_ptr<vulkan::context::Device>    device_;
    std::unique_ptr<vulkan::context::Swapchain> swapchain_;

    float CAM_SPEED = 0.08f;
    float MOUSE_SENSITIVITY = 0.5f;
};

} // namespace app
