module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
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

            vulkan::memory::Image outputImage(device_->get(), device_->physical(), extent,
                                              vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor);

            auto cmdImage = commandPool.getSingleUseBuffer();
            vulkan::memory::Image::setImageLayout(*cmdImage, outputImage.getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            commandPool.submitSingleUse(std::move(cmdImage), device_->computeQueue());
            device_->get().waitIdle();

            vk::Buffer cameraBuffer = camera_->getBuffer();
            // Create the objects in the scene

            std::vector<scene::Object> objects = {};
            scene::Object floor("../../assets/objects/room/floor.obj");
            floor.setSpecular(0.8);
            floor.setColor("../../assets/textures/room/Tiles.jpg");
            objects.push_back(floor);

            scene::Object walls("../../assets/objects/room/room.obj");
            walls.setSpecular(0.8);
            walls.setColor(glm::vec3(0.7));
            objects.push_back(walls);

            scene::Object baseboard("../../assets/objects/room/baseboard.obj");
            baseboard.setSpecular(0.8);
            baseboard.setColor(glm::vec3(0.6));
            objects.push_back(baseboard);

            scene::Object table_base("../../assets/objects/room/table_base.obj");
            table_base.setSpecular(0.8);
            table_base.setColor(glm::vec3(0.9,0.8,0.5));
            objects.push_back(table_base);

            scene::Object table_top("../../assets/objects/room/table_top.obj");
            table_top.setSpecular(0.3);
            table_top.setColor(glm::vec3(0.4));
            objects.push_back(table_top);

            scene::Object picture("../../assets/objects/room/picture.obj");
            picture.setMetallic(0.0);
            picture.setSpecular(0.9);
            picture.setColor("../../assets/textures/room/picture3.jpg");
            objects.push_back(picture);

            std::vector<glm::vec3> chairTransforms = {
                    glm::vec3{0.0,0.0,0.0}, //left
                    glm::vec3{0.0,0.0,0.0},
                    glm::vec3{ -1, 0.0, 0.8}, // back left
                    glm::vec3{ 0.0, 83.0, 0.0},
                    glm::vec3{ -4, 0.0, 0.8},  // back right
                    glm::vec3{ 0.0, 94.0, 0.0},
                    glm::vec3{ 2.5, 0.0, -4.0}, // front left
                    glm::vec3{ 0.0, -92.0, 0.0},
                    glm::vec3{ -0.6, 0.0, -4.0}, // front right
                    glm::vec3{ 0.0, -98.0, 0.0},
                    glm::vec3{ -1.6, 0.0, -4.2}, // right
                    glm::vec3{ 0.0, 181.0, 0.0}
            };

            for (int i = 0; i < chairTransforms.size() / 2; ++i) {
                scene::Object chair_legs("../../assets/objects/room/chair_legs.obj");
                chair_legs.setMetallic(0.9);
                chair_legs.setSpecular(0.1);
                chair_legs.setColor(glm::vec3(0.3));
                chair_legs.rotate(chairTransforms[i * 2 + 1]);
                chair_legs.move(chairTransforms[i * 2]);
                objects.push_back(chair_legs);

                scene::Object chair_seat("../../assets/objects/room/chair_seat.obj");
                chair_seat.setMetallic(0.0);
                chair_seat.setSpecular(0.3);
                chair_seat.setColor(glm::vec3(0.4,0.4,0.5));
                chair_seat.rotate(chairTransforms[i * 2 + 1]);
                chair_seat.move(chairTransforms[i * 2]);
                objects.push_back(chair_seat);
            }

            scene::Object dishes("../../assets/objects/room/dishes.obj");
            dishes.setMetallic(0.0);
            dishes.setSpecular(0.17);
            dishes.setColor(glm::vec3(0.3));
            objects.push_back(dishes);

            scene::Object lightOne("../../assets/objects/room/light.obj");
            lightOne.setMetallic(0.0);
            lightOne.setSpecular(0.8);
            lightOne.setColor(glm::vec3(0.4));
            objects.push_back(lightOne);

            scene::Object lightTwo("../../assets/objects/room/light.obj");
            lightTwo.setMetallic(0.0);
            lightTwo.setSpecular(0.8);
            lightTwo.setColor(glm::vec3(0.4));
            lightTwo.rotate(glm::vec3(0,90,0));
            lightTwo.move(glm::vec3(-1.0,0.0,0.3));
            objects.push_back(lightTwo);

            scene::Object spoon1("../../assets/objects/room/spoon.obj");
            spoon1.setMetallic(1.0);
            spoon1.setSpecular(0.1);
            spoon1.setColor(glm::vec3(0.3));
            objects.push_back(spoon1);

            scene::Object plate1("../../assets/objects/room/cup_plate.obj");
            plate1.setMetallic(0.0);
            plate1.setSpecular(0.2);
            plate1.setColor(glm::vec3(0.85));
            objects.push_back(plate1);

            scene::Object trim1("../../assets/objects/room/trim.obj");
            trim1.setMetallic(1.0);
            trim1.setSpecular(0.2);
            trim1.setColor(glm::vec3(1.0, 0.95,0.0));
            objects.push_back(trim1);

            scene::Object spoon2("../../assets/objects/room/spoon.obj");
            spoon2.setMetallic(1.0);
            spoon2.setSpecular(0.1);
            spoon2.setColor(glm::vec3(0.3));
            spoon2.rotate(glm::vec3(0.0,70.0,0.0));
            spoon2.move(glm::vec3(0.0,0.0,1.0));
            objects.push_back(spoon2);

            scene::Object plate2("../../assets/objects/room/cup_plate.obj");
            plate2.setMetallic(0.0);
            plate2.setSpecular(0.2);
            plate2.setColor(glm::vec3(0.85));
            plate2.rotate(glm::vec3(0.0,70.0,0.0));
            plate2.move(glm::vec3(0.0,0.0,1.0));
            objects.push_back(plate2);

            scene::Object trim2("../../assets/objects/room/trim.obj");
            trim2.setMetallic(1.0);
            trim2.setSpecular(0.2);
            trim2.setColor(glm::vec3(1.0, 0.95,0.0));
            trim2.rotate(glm::vec3(0.0,70.0,0.0));
            trim2.move(glm::vec3(0.0,0.0,1.0));
            objects.push_back(trim2);

            scene::Object spoon3("../../assets/objects/room/spoon.obj");
            spoon3.setMetallic(1.0);
            spoon3.setSpecular(0.1);
            spoon3.setColor(glm::vec3(0.3));
            spoon3.rotate(glm::vec3(0.0,-70.0,0.0));
            spoon3.move(glm::vec3(2.7,0.0,-3.2));
            objects.push_back(spoon3);

            scene::Object plate3("../../assets/objects/room/cup_plate.obj");
            plate3.setMetallic(0.0);
            plate3.setSpecular(0.2);
            plate3.setColor(glm::vec3(0.85));
            plate3.rotate(glm::vec3(0.0,-70.0,0.0));
            plate3.move(glm::vec3(2.7,0.0,-3.2));
            objects.push_back(plate3);

            scene::Object trim3("../../assets/objects/room/trim.obj");
            trim3.setMetallic(1.0);
            trim3.setSpecular(0.2);
            trim3.setColor(glm::vec3(1.0, 0.95,0.0));
            trim3.rotate(glm::vec3(0.0,-70.0,0.0));
            trim3.move(glm::vec3(2.7,0.0,-3.2));
            objects.push_back(trim3);


//            std::vector<scene::Object> objects = {};
//            scene::Object floor("../../assets/objects/basic_geo/cube.obj");
//            floor.setSpecular(0.8);
//            floor.setColor(glm::vec3(0.3));
//            floor.scale(3.0);
//            floor.move(glm::vec3(0.0,-11.165,0.0));
//            objects.push_back(floor);
//
//            scene::Object toyCar("../../assets/objects/toycar/toycar.obj");
//            toyCar.setSpecular(0.05);
//            toyCar.setMetallic(0.0);
//            toyCar.setColor("../../assets/textures/toycar/toycar_color.png");
//            toyCar.scale(50.0);
//            objects.push_back(toyCar);
//
//            scene::Object carStand("../../assets/objects/toycar/car_stand.obj");
//            carStand.setSpecular(0.8);
//            carStand.setMetallic(0.0);
//            carStand.setColor("../../assets/textures/toycar/car_stand_color.png");
//            carStand.scale(50.0);
//            objects.push_back(carStand);

            // Create the area lights in the scene, they start at the origin, facing down the z axis, with side lengths of 1
            std::vector<scene::AreaLight> lights = {};
            // takes in intensity and color
            scene::AreaLight light1(55.0, glm::vec3(1.0,0.94,0.6)); // sun
            light1.scale(glm::vec3(8.0,8.0,8.0));
            light1.move(glm::vec3(15.0,13.0,0.0));
            light1.rotate(glm::vec3(-15.0,00.0,0.0));
            light1.rotate(glm::vec3(0.0,90.0,0.0));
            lights.push_back(light1);

            scene::AreaLight light2(75.0, glm::vec3(1.0,0.97,0.8)); // top light 1
            light2.scale(glm::vec3(0.7,0.7,0.7));
            light2.move(glm::vec3(0.9,3.62,-1.9));
            light2.rotate(glm::vec3(90.0,0.0,0.0));
            light2.rotate(glm::vec3(0.0,0.0,-2.0));
            lights.push_back(light2);

            scene::AreaLight light3(75.0, glm::vec3(1.0,0.97,0.8)); // top light 2
            light3.scale(glm::vec3(0.7,0.7,0.7));
            light3.move(glm::vec3(-2.2,3.62,-1.9));
            light3.rotate(glm::vec3(90.0,0.0,0.0));
            light3.rotate(glm::vec3(0.0,0.0,5.0));
            lights.push_back(light3);

            scene::AreaLight light4(7.0, glm::vec3(0.5,0.8,1.0)); // fill light 1
            light4.scale(glm::vec3(2.0,1.0,1.0));
            light4.move(glm::vec3(3.0,1.0,7.0));
            light4.rotate(glm::vec3(20.0,0.0,0.0));
            lights.push_back(light4);

            scene::AreaLight light5(8.0, glm::vec3(0.5,0.8,1.0)); // fill light 2
            light5.scale(glm::vec3(6.0,3.0,3.0));
            light5.move(glm::vec3(-5.0,4.0,7.0));
            light5.rotate(glm::vec3(20.0,0.0,0.0));
            light5.rotate(glm::vec3(20.0,30.0,0.0));
            lights.push_back(light5);


            auto sceneInfo = std::move(app::setup::CreateScene::createSceneFromObjectsAndLights(*device_, commandPool, objects, lights));
            auto descriptorTextures = std::move(sceneInfo.descriptorTextures);

            auto geometryInfo = std::move(sceneInfo.geoReturnInfo);
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
            vulkan::memory::Buffer lightInfoBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, *device_, lightInfosSize,
                                                                                                    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                                                                    | vk::BufferUsageFlagBits::eTransferDst, lightInfoDataRegions);

            /* JUST FOR TESTING */
            // FOR NOW JUST ONE DESCRIPTOR SET
            vulkan::memory::DescriptorSetLayout layout(device_->get());
            // Create the camera

            layout.addBinding(0, vk::DescriptorType::eStorageImage,  vk::ShaderStageFlagBits::eRaygenKHR); // render image
            layout.addBinding(1, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // TLAS
            layout.addBinding(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);  // camera
            layout.addBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // vertex buffer
            layout.addBinding(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // index buffer
            layout.addBinding(5, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, static_cast<uint32_t>(descriptorTextures.size())); // textures
            layout.addBinding(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); //object infos
            layout.addBinding(7, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR); // light infos
            layout.addBinding(8, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eMissKHR); // hdri

            layout.build();

            // We need this to happen automatically based off of needs
            std::vector<vk::DescriptorPoolSize> poolSizes = {
                    {vk::DescriptorType::eStorageImage, 1},
                    {vk::DescriptorType::eStorageBuffer, 4},
                    {vk::DescriptorType::eUniformBuffer, 1},
                    {vk::DescriptorType::eAccelerationStructureKHR, 1},
                    {vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(descriptorTextures.size()) + 1}
            };

            vulkan::memory::DescriptorPool pool(device_->get(), poolSizes, 3);
            vk::DescriptorSet set = pool.allocate(layout)[0];

            pool.writeImage(set, 0, outputImage.getImageInfo(), vk::DescriptorType::eStorageImage);
            pool.writeAccelerationStructure(set, 1, vk::DescriptorType::eAccelerationStructureKHR, tlas->get());
            pool.writeBuffer(set, 2, cameraBuffer, sizeof(scene::Camera::GPUCameraData), vk::DescriptorType::eUniformBuffer, 0);
            pool.writeBuffer(set, 3, vertexBuffer.get(), sizeof(scene::geometry::Vertex) * vertices.size() , vk::DescriptorType::eStorageBuffer, 0);
            pool.writeBuffer(set, 4, indexBuffer.get(), indices.size() * sizeof(uint32_t), vk::DescriptorType::eStorageBuffer, 0);
            pool.writeImages(set, 5, descriptorTextures, vk::DescriptorType::eCombinedImageSampler);
            pool.writeBuffer(set, 6, objectInfoBuffer.get(), objectInfosSize, vk::DescriptorType::eStorageBuffer, 0);
            pool.writeBuffer(set, 7, lightInfoBuffer.get(), lightInfosSize, vk::DescriptorType::eStorageBuffer, 0);
            pool.writeImage(set, 8, hdriImage->getImageInfoWithSampler(hdriSampler.get()), vk::DescriptorType::eCombinedImageSampler);

            std::vector<vk::DescriptorSetLayout> descriptorLayouts{layout.get()};

            auto raytracePipeline = vulkan::RayTracingPipeline(*device_, descriptorLayouts, "shaders/spv/raygen.rgen.spv",
                                                               "shaders/spv/miss.rmiss.spv", "shaders/spv/shadowMiss.rmiss.spv",
                                                               "shaders/spv/closesthit.rchit.spv");

            int frame = 0;
            float currentTime = window_->getTime();
            uint32_t imageIndex = 0;
            vk::UniqueSemaphore imageAcquiredSemaphore = device_->get().createSemaphoreUnique(vk::SemaphoreCreateInfo());

            scene::SceneInfo sceneData(frame, lights.size(), camera_->getPosition());

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
                sceneData.frame = frame;
                sceneData.camPosition = camera_->getPosition();
                cmd->pushConstants<scene::SceneInfo>(raytracePipeline.getLayout(),
                                                     vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
                                                     0, sceneData);
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