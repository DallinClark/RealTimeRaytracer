module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tiny_obj_loader.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string_view>
#include <filesystem>

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
import vulkan.compute_pipeline;
import vulkan.memory.image;
import vulkan.raytracing.blas;
import vulkan.raytracing.tlas;
import app.window;

import core.file;

import scene.camera;
import scene.scene_info;
import scene.geometry.vertex;
import scene.geometry.triangle;
import scene.object;
import scene.area_light;

import app.setup.geometry_builder;
import app.setup.create_scene;

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

            // Create the images that get rendered too
            std::vector<vk::DescriptorImageInfo> renderImages= {};
            vulkan::memory::Image analyticImage(device_->get(), device_->physical(), extent,
                                              vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image shadowedSampleImage(device_->get(), device_->physical(), extent,
                                                vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image unshadowedSampleImage(device_->get(), device_->physical(), extent,
                                                vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image denoisedShadowedImage(device_->get(), device_->physical(), extent,
                                                    vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image denoisedUnshadowedImage(device_->get(), device_->physical(), extent,
                                                      vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image finalImage(device_->get(), device_->physical(), extent,
                                                        vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            auto cmdImage = commandPool.getSingleUseBuffer();
            for (vulkan::memory::Image* image : { &analyticImage, &shadowedSampleImage, &unshadowedSampleImage, &denoisedShadowedImage, &denoisedUnshadowedImage, &finalImage }) {
                vulkan::memory::Image::setImageLayout(*cmdImage, image->getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                renderImages.push_back(image->getImageInfo());
            }
            commandPool.submitSingleUse(std::move(cmdImage), device_->computeQueue());
            device_->get().waitIdle();

            vk::Buffer cameraBuffer = camera_->getBuffer();

            // Create the objects in the scene
            std::vector<std::shared_ptr<scene::Object>> objects = {};

            auto ground = std::make_shared<scene::Object>("../../assets/objects/basic_geo/cube.obj");
            ground->setSpecular(0.8);
            ground->setColor("../../assets/textures/basic/concrete2.jpg");
            ground->scale(50.0);
            ground->move(glm::vec3(10,-28.6,-10.0));
            objects.push_back(ground);

            auto sphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/sphere.obj");
            sphere->setSpecular(0.3);
            sphere->setColor(glm::vec3(1.0, 0.6, 0.6));
            sphere->scale(0.6);
            sphere->move(glm::vec3(-4.5,-2.1,-1.20));
            objects.push_back(sphere);

            auto moon = std::make_shared<scene::Object>("../../assets/objects/basic_geo/moon.obj");
            moon->setSpecular(0.8);
            moon->setColor("../../assets/textures/basic/moon.png");
            moon->scale(0.015);
            moon->move(glm::vec3(0.0,-2.3,2.0));
            objects.push_back(moon);

            auto metalSphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/sphere.obj");
            metalSphere->setSpecular(0.08);
            metalSphere->setColor(glm::vec3(0.7, 1.0, 0.6));
            metalSphere->scale(0.5);
            metalSphere->setMetallic(0.94);
            metalSphere->move(glm::vec3(4.0,-2.3,-3.3));
            objects.push_back(metalSphere);

            auto holeSphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/hole_sphere.obj");
            holeSphere->setSpecular(0.6);
            holeSphere->setColor("../../assets/textures/basic/sphere_hole_color.png");
            holeSphere->scale(11.5);
            holeSphere->move(glm::vec3(0.0,2.3,-12.3));
            objects.push_back(holeSphere);

            std::vector<std::shared_ptr<scene::AreaLight>> lights = {};
            // takes in intensity, color, if it's two sided, and path (optional, defaults to square)

            auto light1 = std::make_shared<scene::AreaLight>(2.0, glm::vec3(1.0,1.0, 1.0), false);
            light1->move(glm::vec3(8.0,-1.5,0.0));
            light1->scale(glm::vec3(4.0,2.5,1.0));
            light1->rotate(glm::vec3(0.0,90.0,0.0));
            light1->rotate(glm::vec3(0.0,0.0,165.0));
            lights.push_back(light1);
//
            auto light2 = std::make_shared<scene::AreaLight>(2.0, glm::vec3(1.0,1.0, 0.2), false);
            light2->scale(glm::vec3(4.0,2.5,1.0));
            light2->move(glm::vec3(-10.0,-1.5,0.0));
            light2->rotate(glm::vec3(0.0,90.0,0.0));
            light2->rotate(glm::vec3(0.0,0.0,15.0));
            lights.push_back(light2);

            auto sphereLight = std::make_shared<scene::AreaLight>(3.0, glm::vec3(1.0,1.0, 1.0), false, "../../assets/objects/lights/sphere.obj");
            sphereLight->move(glm::vec3(0.0, 2.0, -12.0));
            sphereLight->scale(glm::vec3(0.5,0.5,0.5));
            lights.push_back(sphereLight);

            auto sceneInfo = app::setup::CreateScene::createSceneFromObjectsAndLights(*device_, commandPool, objects, lights);
            auto descriptorTextures = std::move(sceneInfo.descriptorTextures);

            auto& geometryInfo = sceneInfo.geoReturnInfo;
            std::vector<glm::vec3>                     vertexPositions = geometryInfo.vertexPositions;
            std::vector<scene::geometry::Vertex>       vertices = geometryInfo.vertices;
            std::vector<uint32_t>                      indices = geometryInfo.indices;
            std::unique_ptr<vulkan::raytracing::TLAS>  tlas = std::move(geometryInfo.tlas);
            vulkan::memory::Buffer                     vertexPositionBuffer = std::move(geometryInfo.vertexBuffer);
            vulkan::memory::Buffer                     indexBuffer = std::move(geometryInfo.indexBuffer);

            // create shader vertex buffer
            vk::DeviceSize verticesSize = sizeof(scene::geometry::Vertex) * vertices.size();
            std::vector<vulkan::memory::Buffer::FillRegion> vertexDataRegions {
                    {vertices.data() , verticesSize, 0},
            };
            vulkan::memory::Buffer vertexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, *device_, verticesSize,
                                                                                                  vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                                                                  | vk::BufferUsageFlagBits::eTransferDst, vertexDataRegions);

            auto hdriImage = core::file::createTextureImage(*device_.get(), "../../assets/textures/hdris/sky4k.hdr", commandPool, false);
            vulkan::memory::ImageSampler hdriSampler(*device_.get());

            // Object Info Buffer
            std::vector<scene::Object::GPUObjectInfo> objectInfos = sceneInfo.GPUObjects;
            vk::DeviceSize objectInfosSize = objectInfos.size() * sizeof(scene::Object::GPUObjectInfo);
            std::vector<vulkan::memory::Buffer::FillRegion> objectInfoDataRegions {
                    {objectInfos.data() , objectInfosSize, 0},
            };
            vulkan::memory::Buffer objectInfoBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, *device_, objectInfosSize,
                                                                                                      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                                                                      | vk::BufferUsageFlagBits::eTransferDst, objectInfoDataRegions);

            // Light Info Buffer
            std::vector<scene::AreaLight::GPUAreaLightInfo> lightInfos = sceneInfo.GPUAreaLights;
            vk::DeviceSize lightInfosSize = lightInfos.size() * sizeof(scene::AreaLight::GPUAreaLightInfo);
            std::vector<vulkan::memory::Buffer::FillRegion> lightInfoDataRegions {
                    {lightInfos.data() , lightInfosSize, 0},
            };
            vulkan::memory::Buffer lightInfoBuffer (*device_, lightInfosSize,vk::BufferUsageFlagBits::eStorageBuffer,
                                                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            lightInfoBuffer.fill(lightInfoDataRegions);

            // Create two descriptor sets, one used in only the raytracing pipeline
            // and one that is also accessed in the compute pipeline
            vulkan::memory::DescriptorSetLayout rayAndComputeLayout(device_->get());
            vulkan::memory::DescriptorSetLayout rayTraceOnlyLayout(device_->get());

            std::vector<std::vector<vk::DescriptorPoolSize>> poolGroups = {};

            // add images used for rendering to the ray and compute layout
            for (int i = 0; i < renderImages.size(); ++i) {
                rayAndComputeLayout.addBinding(i, vk::DescriptorType::eStorageImage,  vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eCompute);
            }
            rayAndComputeLayout.build();
            std::vector<vk::DescriptorPoolSize> rayAndComputePoolSizes = rayAndComputeLayout.getPoolSizes();

            // add everything needed for the ray tracing layout
            rayTraceOnlyLayout.addBinding(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // TLAS
            rayTraceOnlyLayout.addBinding(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // camera
            rayTraceOnlyLayout.addBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // vertex buffer
            rayTraceOnlyLayout.addBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // index buffer
            rayTraceOnlyLayout.addBinding(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, static_cast<uint32_t>(descriptorTextures.size())); // textures
            rayTraceOnlyLayout.addBinding(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); //object infos
            rayTraceOnlyLayout.addBinding(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // light infos
            rayTraceOnlyLayout.addBinding(7, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eMissKHR); // hdri
            rayTraceOnlyLayout.build();
            poolGroups.push_back(rayTraceOnlyLayout.getPoolSizes());


            std::vector<vk::DescriptorPoolSize> poolSizesCombined = vulkan::memory::DescriptorSetLayout::combinePoolSizes(poolGroups);

            vulkan::memory::DescriptorPool pool(device_->get(), poolSizesCombined, 2);

            std::vector<vulkan::memory::DescriptorSetLayout*> layouts = {&rayAndComputeLayout, &rayTraceOnlyLayout};
            std::vector<vk::DescriptorSet> sets = pool.allocate(layouts);
            vk::DescriptorSet rayAndComputeSet = sets[0];
            vk::DescriptorSet rayTraceSet      = sets[1];

            for (int i = 0; i < renderImages.size(); ++i) {
                pool.writeImage(rayAndComputeSet, i, renderImages[i], vk::DescriptorType::eStorageImage);
            }

            pool.writeAccelerationStructure(rayTraceSet, 0, vk::DescriptorType::eAccelerationStructureKHR, tlas->get());
            pool.writeBuffer(rayTraceSet, 1, cameraBuffer, sizeof(scene::Camera::GPUCameraData), vk::DescriptorType::eUniformBuffer, 0);
            pool.writeBuffer(rayTraceSet, 2, vertexBuffer.get(), sizeof(scene::geometry::Vertex) * vertices.size() , vk::DescriptorType::eStorageBuffer, 0);
            pool.writeBuffer(rayTraceSet, 3, indexBuffer.get(), indices.size() * sizeof(uint32_t), vk::DescriptorType::eStorageBuffer, 0);
            pool.writeImages(rayTraceSet, 4, descriptorTextures, vk::DescriptorType::eCombinedImageSampler);
            pool.writeBuffer(rayTraceSet, 5, objectInfoBuffer.get(), objectInfosSize, vk::DescriptorType::eStorageBuffer, 0);
            pool.writeBuffer(rayTraceSet, 6, lightInfoBuffer.get(), lightInfosSize, vk::DescriptorType::eStorageBuffer, 0);
            pool.writeImage(rayTraceSet, 7, hdriImage->getImageInfoWithSampler(hdriSampler.get()), vk::DescriptorType::eCombinedImageSampler);

            std::vector<vk::DescriptorSetLayout> raytraceLayouts{rayAndComputeLayout.get(), rayTraceOnlyLayout.get()};

            auto raytracePipeline = vulkan::RayTracingPipeline(*device_, raytraceLayouts, "shaders/spv/raygen.rgen.spv",
                                                               "shaders/spv/miss.rmiss.spv", "shaders/spv/shadowMiss.rmiss.spv",
                                                               "shaders/spv/closesthit.rchit.spv");

            std::vector<vk::DescriptorSetLayout> computeLayouts{rayAndComputeLayout.get()};
            auto denoisePipeline = vulkan::ComputePipeline(*device_, computeLayouts, "shaders/spv/denoise.comp.spv");
            auto combinePipeline = vulkan::ComputePipeline(*device_, computeLayouts, "shaders/spv/combine.comp.spv");

            int frame = 0;
            float currentTime = window_->getTime();
            uint32_t imageIndex = 0;
            vk::UniqueSemaphore imageAcquiredSemaphore = device_->get().createSemaphoreUnique(vk::SemaphoreCreateInfo());

            scene::SceneInfo sceneData(frame, lights.size(), camera_->getPosition());

            // Main Loop
            while (!window_->shouldClose()) {
                moon->move(glm::vec3(sin(frame * 0.02f + glm::pi<float>() / 2) * 0.1, 0.0, 0.0));
                tlas->updateTransform(moon->getInstanceIndex(), moon->getTransform());

                holeSphere->rotate(glm::vec3(0.0,0.5,0.0));
                tlas->updateTransform(holeSphere->getInstanceIndex(), holeSphere->getTransform());

                // TODO TODO TODO These lines are discusting hard coded crap, that you need going to change
                sphereLight->move(glm::vec3(0.0, 0.0, (sin(frame * 0.01f) * 0.1)));
                tlas->updateTransform(sphereLight->getInstanceIndex(), sphereLight->getTransform());
                scene::AreaLight::GPUAreaLightInfo info = sphereLight->getGPUInfo();
                lightInfoBuffer.fill(&info, sizeof(scene::AreaLight::GPUAreaLightInfo), 2 * sizeof(scene::AreaLight::GPUAreaLightInfo));

                float newTime = window_->getTime();
                float frameTime = newTime - currentTime;
                currentTime = newTime;
                camera_->updateGPUData();
                glfwPollEvents();
                window_->processInput(newTime, CAM_SPEED);

                imageIndex = swapchain_->acquireNextImage(imageAcquiredSemaphore);

                auto cmd = commandPool.getSingleUseBuffer();
                tlas->refit(*device_, commandPool);
                cmd->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.get());
                cmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.getLayout(), 0, rayAndComputeSet, nullptr);
                cmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.getLayout(), 1,      rayTraceSet, nullptr);
                sceneData.frame = frame;
                sceneData.camPosition = camera_->getPosition();
                cmd->pushConstants<scene::SceneInfo>(raytracePipeline.getLayout(),vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, 0, sceneData);
                raytracePipeline.recordTraceRays(*cmd, extent);

                // Denoise the sampled images
                int computeImageIndex = 0;
                cmd->bindPipeline(vk::PipelineBindPoint::eCompute, denoisePipeline.get());
                cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, denoisePipeline.getLayout(), 0, rayAndComputeSet, nullptr);
                cmd->pushConstants(denoisePipeline.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(int), &computeImageIndex);

                // Tell the compute shader how many different work groups to run, 8 wide is set in the shader
                uint32_t groupCountX = (extent.width  + 7) / 8;
                uint32_t groupCountY = (extent.height + 7) / 8;
                cmd->dispatch(groupCountX, groupCountY, 1);

                computeImageIndex = 1;
                cmd->pushConstants(denoisePipeline.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(int), &computeImageIndex);
                cmd->dispatch(groupCountX, groupCountY, 1);

                // Finally, combine the images
                cmd->bindPipeline(vk::PipelineBindPoint::eCompute, combinePipeline.get());
                cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, combinePipeline.getLayout(), 0, rayAndComputeSet, nullptr);
                cmd->dispatch(groupCountX, groupCountY, 1);

                vk::Image srcImage = finalImage.getImage();
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
                // Wait Idle is slow
                device_->presentQueue().waitIdle();
                ++frame;
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