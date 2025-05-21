module;
#include <vector>
#include <string>
#include <fstream>
#include <vulkan/vulkan.hpp>

import core.file;

export module vulkan.ray_tracing_pipeline;
namespace vulkan::context {

    export class RayTracingPipeline {
    public:
        RayTracingPipeline(const Device& device);

        void loadShaders(std::string_view rgen, std::string_view rmiss, std::string_view rchit);
        void createPipelineLayout();
        void createPipeline();
        void createShaderBindingTable();
        void recordTraceRays(const CommandBuffer& cmdBuf, vk::Extent2D extent);
        vk::UniquePipeline createRayTracingPipeline(const Device& device,
                                                    vk::PipelineCache pipelineCache,
                                                    const vk::RayTracingPipelineCreateInfoKHR& createInfo,
                                                    const vk::AllocationCallbacks* allocator = nullptr);

        [[nodiscard]] vk::Pipeline get() const noexcept { return pipeline_.get(); }

    private:
        const Device& device_;

        // Shader modules
        vk::UniqueShaderModule rgen_, rmiss_, rchit_;

        // Pipeline
        vk::UniquePipelineLayout pipelineLayout_;
        vk::UniquePipeline pipeline_;

        // Shader Binding Table (SBT) buffer
        vk::UniqueBuffer sbtBuffer_;
        vk::UniqueDeviceMemory sbtMemory_;

        // Ray tracing props
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{};

        // Internal utils
        vk::UniqueShaderModule createShaderModule(std::string_view path);
        void queryRayTracingProperties();
    };

    // Implemented functions
    inline RayTracingPipeline::RayTracingPipeline(const Device& device)
            : device_(device) {
        queryRayTracingProperties();
    }

    inline void RayTracingPipeline::queryRayTracingProperties() {
        rtProps_.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
        vk::PhysicalDeviceProperties2 props2{};
        props2.sType = vk::StructureType::ePhysicalDeviceProperties2;
        props2.pNext = &rtProps_;
        device_.physical().getProperties2(&props2);
        core::log::debug("RT Shader Group Handle Size: {}", rtProps_.shaderGroupHandleSize);
    }

    inline void RayTracingPipeline::loadShaders(std::string_view rgen, std::string_view rmiss, std::string_view rchit) {
        rgen_ = createShaderModule(rgen);
        rmiss_ = createShaderModule(rmiss);
        rchit_ = createShaderModule(rchit);
    }

    inline vk::UniqueShaderModule RayTracingPipeline::createShaderModule(std::string_view path) {
        // Load SPIR-V shader file into vector<char>
        // You’ll need your own file utility here
        std::vector<char> code = core::loadBinaryFile(path); // <-- implement this
        vk::ShaderModuleCreateInfo info{
                {}, code.size(),
                reinterpret_cast<const uint32_t*>(code.data())
        };
        return createShaderModuleUnique(info);
    }
    inline void RayTracingPipeline::createPipelineLayout() {
        vk::PipelineLayoutCreateInfo layoutInfo{};
        pipelineLayout_ = device_.createPipelineLayoutUnique(layoutInfo);
    }

    inline vk::UniquePipeline RayTracingPipeline::createRayTracingPipelineKHRUnique(
            const vk::Device& device,
            vk::PipelineCache pipelineCache,
            const vk::RayTracingPipelineCreateInfoKHR& createInfo,
            const vk::AllocationCallbacks* allocator = nullptr) {
        vk::Pipeline pipeline;
        auto result = device.createRayTracingPipelinesKHR(pipelineCache, 1, &createInfo, allocator, &pipeline);
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create ray tracing pipeline");
        }
        return vk::UniquePipeline(pipeline, device, allocator);
    }

    inline void RayTracingPipeline::createPipeline() {
        // Define shader stages
        std::vector<vk::PipelineShaderStageCreateInfo> stages = {
                { {}, vk::ShaderStageFlagBits::eRaygenKHR, rgen_.get(), "main" },
                { {}, vk::ShaderStageFlagBits::eMissKHR,   rmiss_.get(), "main" },
                { {}, vk::ShaderStageFlagBits::eClosestHitKHR, rchit_.get(), "main" }
        };

        // Shader groups
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups = {
                { vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
                { vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
                { vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR }
        };

        vk::RayTracingPipelineCreateInfoKHR pipelineInfo{
                {},                          // flags
                static_cast<uint32_t>(stages.size()),
                stages.data(),
                static_cast<uint32_t>(groups.size()),
                groups.data(),
                {},                          // maxRecursionDepth, layout
                pipelineLayout_.get(),       // pipeline layout
                {}, {},                      // base pipeline
                0
        };

        pipeline_ = device_.createRayTracingPipelineKHRUnique({}, {}, pipelineInfo).value;
    }

    inline void RayTracingPipeline::createShaderBindingTable() {
        // create shader binding table here
    }

    inline void RayTracingPipeline::recordTraceRays(const CommandBuffer& cmdBuf, vk::Extent2D extent) {
        // Fill StridedDeviceAddressRegionKHR for rgen, miss, chit, etc.
        // call traceRays and record them in a buffer
    }

}