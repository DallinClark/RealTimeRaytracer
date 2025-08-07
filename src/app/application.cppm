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
import app.structs;

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

            vulkan::memory::Image normalImage(device_->get(), device_->physical(), extent,
                                             vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            vulkan::memory::Image positionImage(device_->get(), device_->physical(), extent,
                                             vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            auto cmdImage = commandPool.getSingleUseBuffer();
            for (vulkan::memory::Image* image : { &analyticImage, &shadowedSampleImage, &unshadowedSampleImage, &denoisedShadowedImage, &denoisedUnshadowedImage, &finalImage,
                                                  &normalImage, &positionImage}) {
                vulkan::memory::Image::setImageLayout(*cmdImage, image->getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
                renderImages.push_back(image->getImageInfo());
            }
            commandPool.submitSingleUse(std::move(cmdImage), device_->computeQueue());

            vk::Buffer cameraBuffer = camera_->getBuffer();

            // Create the objects in the scene
            std::vector<std::shared_ptr<scene::Object>> objects = {};

//            auto ground = std::make_shared<scene::Object>("../../assets/objects/basic_geo/cube.obj");
//            ground->setSpecular(0.8);
//            ground->setColor("../../assets/textures/basic/concrete2.jpg");
//            ground->scale(50.0);
//            ground->move(glm::vec3(10,-28.6,-10.0));
//            objects.push_back(ground);
//
//            auto sphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/sphere.obj");
//            sphere->setSpecular(0.3);
//            sphere->setColor(glm::vec3(1.0, 0.6, 0.6));
//            sphere->scale(0.6);
//            sphere->move(glm::vec3(-6.5,-2.1,-1.20));
//            objects.push_back(sphere);
//
//            auto moon = std::make_shared<scene::Object>("../../assets/objects/basic_geo/moon.obj");
//            moon->setSpecular(0.2);
//            moon->setColor("../../assets/textures/basic/moon.png");
//            moon->scale(0.020);
//            moon->move(glm::vec3(0.0,-2.0,2.0));
//            objects.push_back(moon);
//
//            auto metalSphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/sphere.obj");
//            metalSphere->setSpecular(0.92);
//            metalSphere->setColor(glm::vec3(0.7, 1.1, 0.6));
//            metalSphere->scale(0.7);
//            metalSphere->setMetallic(0.94);
//            metalSphere->move(glm::vec3(5.0,-2.1,-3.3));
//            objects.push_back(metalSphere);
//
//            auto holeSphere = std::make_shared<scene::Object>("../../assets/objects/basic_geo/hole_sphere.obj");
//            holeSphere->setSpecular(0.4);
//            holeSphere->setColor("../../assets/textures/basic/sphere_hole_color.png");
//            holeSphere->scale(13.5);
//            holeSphere->move(glm::vec3(0.0,2.1,-12.3));
//            objects.push_back(holeSphere);

            std::vector<std::shared_ptr<scene::AreaLight>> lights = {};
            // takes in intensity, color, if it's two sided, and path (optional, defaults to square)

            auto light1 = std::make_shared<scene::AreaLight>(9.0, glm::vec3(0.8,0.5, 0.2), false); // THIS ONE
            light1->move(glm::vec3(-1600.0,2500.5,-500.0));
            light1->scale(glm::vec3(1300.0,750.5,100.0));
            light1->rotate(glm::vec3(0.0,90.0,0.0));
            light1->rotate(glm::vec3(0.0,0.0,55.0));
            lights.push_back(light1);

            auto light2 = std::make_shared<scene::AreaLight>(3.0, glm::vec3(0.3,0.3, 0.5), false); // THIS ONE
            light2->move(glm::vec3(-1600.0,200.5,-400.0));
            light2->scale(glm::vec3(500.0,200.5,100.0));
            light2->rotate(glm::vec3(0.0,135.0,0.0));
            light2->rotate(glm::vec3(0.0,0.0,0.0));
            lights.push_back(light2);

//            auto light2 = std::make_shared<scene::AreaLight>(2.0, glm::vec3(0.3,0.3, 1.0), false);
//            light2->move(glm::vec3(-800.0,100.5,0.0));
//            light2->scale(glm::vec3(600.0,200.5,100.0));
//            light2->rotate(glm::vec3(0.0,90.0,0.0));
//            lights.push_back(light2);

//            auto light1 = std::make_shared<scene::AreaLight>(2.0, glm::vec3(1.0,1.0, 1.0), false);
//            light1->scale(glm::vec3(4.0,2.5,1.0));
//            light1->move(glm::vec3(8.0,-1.5,0.0));
//            light1->rotate(glm::vec3(0.0,90.0,0.0));
//            light1->rotate(glm::vec3(0.0,0.0,165.0));
//            lights.push_back(light1);
//
////
//            auto light2 = std::make_shared<scene::AreaLight>(2.0, glm::vec3(1.0,1.0, 0.2), false);
//            light2->scale(glm::vec3(4.0,2.5,1.0));
//            light2->move(glm::vec3(-10.0,-1.5,0.0));
//            light2->rotate(glm::vec3(0.0,90.0,0.0));
//            light2->rotate(glm::vec3(0.0,0.0,15.0));
//            lights.push_back(light2);
//
//
//            auto sphereLight = std::make_shared<scene::AreaLight>(2.5, glm::vec3(1.0,1.0, 1.0), false,false, "../../assets/objects/lights/sphere.obj");
//            sphereLight->move(glm::vec3(0.0, 2.5, -12.0));
//            sphereLight->scale(glm::vec3(0.5,0.5,0.5));
//            lights.push_back(sphereLight);


            std::vector<std::pair<std::string, std::string>> objMtlPairs = {
                    {"../../assets/bistro/Exterior/exterior.obj", "../../assets/bistro/Exterior/"}
            };

            auto sceneInfo = app::setup::CreateScene::createSceneFromObjectsAndLights(*device_, commandPool, objects, objMtlPairs, lights);
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
            rayTraceOnlyLayout.addBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR); // vertex buffer
            rayTraceOnlyLayout.addBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR); // index buffer
            rayTraceOnlyLayout.addBinding(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, static_cast<uint32_t>(descriptorTextures.size())); // textures
            rayTraceOnlyLayout.addBinding(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR); //object infos
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
                                                               "shaders/spv/miss.rmiss.spv",
                                                               "shaders/spv/closesthit.rchit.spv", "shaders/spv/opacity.rahit.spv");

            std::vector<vk::DescriptorSetLayout> computeLayouts{rayAndComputeLayout.get()};
            auto denoisePipeline = vulkan::ComputePipeline(*device_, computeLayouts, "shaders/spv/denoise.comp.spv", true, sizeof(DenoisingInfo));
            auto combinePipeline = vulkan::ComputePipeline(*device_, computeLayouts, "shaders/spv/combine.comp.spv", true, sizeof(int));

            int frame = 0;
            float currentTime = window_->getTime();
            uint32_t imageIndex = 0;

            uint32_t groupCountX = (extent.width  + 7) / 8;
            uint32_t groupCountY = (extent.height + 7) / 8;

            scene::SceneInfo sceneData(frame, lights.size(), camera_->getPosition());

            const int numCmdBufsNeeded = 1 /*raytrace*/ + 2 /*denoise*/ + 1 /*combine*/;
            auto cmdBuffers = commandPool.allocateCommandBuffers(numCmdBufsNeeded);

            auto& raytraceCmd = cmdBuffers[0];
            auto& denoiseCmd1 = cmdBuffers[1];
            auto& denoiseCmd2 = cmdBuffers[2];
            auto& combineCmd = cmdBuffers[3];

            vk::UniqueSemaphore imageAcquiredSemaphore = device_->get().createSemaphoreUnique({});


            while (!window_->shouldClose()) {
                device_->get().waitIdle();
                float newTime = window_->getTime();
                float frameTime = newTime - currentTime;
                currentTime = newTime;
                camera_->updateGPUData();
                glfwPollEvents();
                window_->processInput(newTime, CAM_SPEED);

                // === Raytrace ===
                raytraceCmd->reset();
                raytraceCmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
                raytraceCmd->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.get());
                raytraceCmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.getLayout(), 0, rayAndComputeSet, nullptr);
                raytraceCmd->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, raytracePipeline.getLayout(), 1, rayTraceSet, nullptr);

                sceneData.frame = frame;
                sceneData.camPosition = camera_->getPosition();
                raytraceCmd->pushConstants<scene::SceneInfo>(
                        raytracePipeline.getLayout(),
                        vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR,
                        0, sceneData
                );

                raytracePipeline.recordTraceRays(*raytraceCmd, extent);
                raytraceCmd->end();

                vk::SubmitInfo raytraceSubmit = {};
                //raytraceSubmit.waitSemaphoreCount = 1;
                //raytraceSubmit.pWaitSemaphores = &combineFinishedSemaphore.get();
                //vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eComputeShader;
                //raytraceSubmit.pWaitDstStageMask = &waitStage;
                raytraceSubmit.commandBufferCount = 1;
                raytraceSubmit.pCommandBuffers = &raytraceCmd.get();
                //raytraceSubmit.signalSemaphoreCount = 1;
                //raytraceSubmit.pSignalSemaphores = &raytraceDoneSemaphore.get();

                device_->computeQueue().submit(raytraceSubmit);

                // === Denoise Passes ===
                int denoisingOutput = 1;
                int finalSemaphoreIndex = 0;

                for (int i = 0; i < NUM_DENOISING_ITERATIONS; ++i) {
                    device_->get().waitIdle();

                    auto& denoiseCmd = (i % 2 == 0) ? denoiseCmd1 : denoiseCmd2;

                    denoiseCmd->reset();
                    denoiseCmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                    denoiseCmd->bindPipeline(vk::PipelineBindPoint::eCompute, denoisePipeline.get());
                    denoiseCmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, denoisePipeline.getLayout(), 0, rayAndComputeSet, nullptr);

                    DenoisingInfo denoiseInfo((i + 1) * DENOISING_STRENGTH, 1.0f, 0.001f, 0.001f, denoisingOutput, 0);
                    denoiseCmd->pushConstants(denoisePipeline.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(DenoisingInfo), &denoiseInfo);
                    denoiseCmd->dispatch(groupCountX, groupCountY, 1);

                    denoiseInfo.isShadowedImage = 1;
                    denoiseCmd->pushConstants(denoisePipeline.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(DenoisingInfo), &denoiseInfo);
                    denoiseCmd->dispatch(groupCountX, groupCountY, 1);

                    denoiseCmd->end();

                    // Fixed semaphore logic - each iteration signals the SAME semaphore that the next will wait on
                    uint32_t currentSemaphoreIndex = i % 2;
                    uint32_t nextSemaphoreIndex = (i + 1) % 2;
                    finalSemaphoreIndex = currentSemaphoreIndex; // Track which semaphore the final iteration signals

                    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eComputeShader };
                    vk::PipelineStageFlags waitStagesRay[] = { vk::PipelineStageFlagBits::eRayTracingShaderKHR };

                    // Capture the command buffer reference to avoid issues with changing references
                    vk::CommandBuffer cmdBuf = denoiseCmd.get();

                    vk::SubmitInfo denoiseSubmit = {};
                    denoiseSubmit.commandBufferCount = 1;
                    denoiseSubmit.pCommandBuffers = &cmdBuf;

                    device_->computeQueue().submit(denoiseSubmit);

                    denoisingOutput = 1 - denoisingOutput;
                }

                // === Combine Pass ===
                device_->get().waitIdle();

                combineCmd->reset();
                combineCmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                combineCmd->bindPipeline(vk::PipelineBindPoint::eCompute, combinePipeline.get());
                combineCmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, combinePipeline.getLayout(), 0, rayAndComputeSet, nullptr);
                combineCmd->pushConstants(combinePipeline.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(int), &denoisingOutput);
                combineCmd->dispatch(groupCountX, groupCountY, 1);

                // Acquire image
                uint32_t imageIndex = swapchain_->acquireNextImage(imageAcquiredSemaphore);

                vk::Image srcImage = finalImage.getImage();
                vk::Image dstImage = swapchain_->getImage(imageIndex);

                vulkan::memory::Image::setImageLayout(*combineCmd, srcImage, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
                vulkan::memory::Image::setImageLayout(*combineCmd, dstImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                vulkan::memory::Image::copyImage(*combineCmd, srcImage, dstImage, extent);
                vulkan::memory::Image::setImageLayout(*combineCmd, srcImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
                vulkan::memory::Image::setImageLayout(*combineCmd, dstImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

                combineCmd->end();

                vk::SubmitInfo combineSubmit = {};
                combineSubmit.commandBufferCount = 1;
                combineSubmit.pCommandBuffers = &combineCmd.get();


                device_->computeQueue().submit(combineSubmit);

                // === Present ===
                vk::PresentInfoKHR presentInfo;
                vk::SwapchainKHR swapchain = swapchain_->get();
                std::vector<vk::SwapchainKHR> swapchains = { swapchain };
                presentInfo.setSwapchains(swapchains);
                presentInfo.setImageIndices(imageIndex);
                presentInfo.setWaitSemaphores(*imageAcquiredSemaphore);

                if (device_->presentQueue().presentKHR(presentInfo) != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to present.");
                }
                ++frame;
            }

            device_->get().waitIdle();
            core::log::info("Main loop exited");
        }



    private:
        std::unique_ptr<app::Window>   window_{nullptr};
        std::unique_ptr<scene::Camera> camera_{nullptr};

        std::unique_ptr<vulkan::context::Instance>  instance_;
        std::unique_ptr<vulkan::context::Surface>   surface_;
        std::unique_ptr<vulkan::context::Device>    device_;
        std::unique_ptr<vulkan::context::Swapchain> swapchain_;

        float CAM_SPEED = 10.5f;
        float MOUSE_SENSITIVITY = 0.5f;

        const int NUM_DENOISING_ITERATIONS = 4;
        const int DENOISING_STRENGTH = 1;
    };

} // namespace app