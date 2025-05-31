module;
#include <vector>
#include <string>
#include <fstream>
#include <vulkan/vulkan.hpp>

export module vulkan.ray_tracing_pipeline;

import core.file;
import core.log;
import vulkan.context.device;
import vulkan.context.command_pool;

namespace vulkan {

    export class RayTracingPipeline {
    public:
        RayTracingPipeline(const context::Device &device, std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                           std::string_view rgenPath, std::string_view rmissPath,
                           std::string_view rchitPath);

        void createShaderModules(std::string_view rgen, std::string_view rmiss, std::string_view rchit);

        void createShaderGroups();

        void createPipelineLayout();

        void createPipeline();
        //        void createShaderBindingTable();
        //        void recordTraceRays(const CommandBuffer& cmdBuf, vk::Extent2D extent);
        //        vk::UniquePipeline createRayTracingPipeline(const vk::Device& device,
        //                                                    vk::PipelineCache pipelineCache,
        //                                                    const vk::RayTracingPipelineCreateInfoKHR& createInfo,
        //                                                    const vk::AllocationCallbacks* allocator = nullptr);

        [[nodiscard]] vk::Pipeline get() const noexcept { return pipeline_.get(); }

    private:
        const context::Device &device_;
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts_;

        // Shader modules
        vk::UniqueShaderModule rgen_, rmiss_, rchit_;
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups_;
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages_;

        // Pipeline
        vk::UniquePipelineLayout pipelineLayout_;
        vk::UniquePipeline pipeline_;

        // Shader Binding Table (SBT) buffer
        vk::UniqueBuffer sbtBuffer_;
        vk::UniqueDeviceMemory sbtMemory_;

        // Ray tracing props
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{};

        // Internal utils
        inline vk::UniqueShaderModule createShaderModule(std::string_view path);
        //void queryRayTracingProperties();
    };


    RayTracingPipeline::RayTracingPipeline(const context::Device &device,
                                           std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                                           std::string_view rgenPath, std::string_view rmissPath,
                                           std::string_view rchitPath) : device_{device},
                                                                         descriptorSetLayouts_(descriptorSetLayouts) {
        createShaderModules(rgenPath, rmissPath, rchitPath);
        createShaderGroups();
        createPipelineLayout();
        createPipeline();
    }

    void
    RayTracingPipeline::createShaderModules(std::string_view rgen, std::string_view rmiss, std::string_view rchit) {
        rgen_ = createShaderModule(rgen);
        rmiss_ = createShaderModule(rmiss);
        rchit_ = createShaderModule(rchit);
    }

    vk::UniqueShaderModule RayTracingPipeline::createShaderModule(std::string_view path) {
        // Load SPIR-V shader file into vector<char>
        std::vector<char> code = core::file::loadBinaryFile(path);
        vk::ShaderModuleCreateInfo createInfo{
                {}, code.size(),
                reinterpret_cast<const uint32_t *>(code.data())
        };
        return device_.get().createShaderModuleUnique(createInfo);
    }

    void RayTracingPipeline::createShaderGroups() {
        // different stages in the pipeline, linked to the groups
        shaderStages_ = {
                {{}, vk::ShaderStageFlagBits::eRaygenKHR,     rgen_.get(),  "main"},
                {{}, vk::ShaderStageFlagBits::eMissKHR,       rmiss_.get(), "main"},
                {{}, vk::ShaderStageFlagBits::eClosestHitKHR, rchit_.get(), "main"}
        };
        shaderGroups_ = {
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           0,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           1,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
                {vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 2,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}
        };
    }

    void RayTracingPipeline::createPipelineLayout() {
        vk::PipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.setLayoutCount = descriptorSetLayouts_.size();
        layoutInfo.pSetLayouts = &descriptorSetLayouts_[0];
        pipelineLayout_ = device_.get().createPipelineLayoutUnique(layoutInfo);
    }

    void RayTracingPipeline::createPipeline() {

        vk::RayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages_.size());
        pipelineInfo.pStages = shaderStages_.data();
        pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups_.size());
        pipelineInfo.pGroups = shaderGroups_.data();
        pipelineInfo.layout = pipelineLayout_.get();

        pipeline_ = device_.get().createRayTracingPipelineKHRUnique({}, {}, pipelineInfo).value;
    }
    // Implemented functions

//
//    inline void RayTracingPipeline::queryRayTracingProperties() {
//        rtProps_.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
//        vk::PhysicalDeviceProperties2 props2{};
//        props2.sType = vk::StructureType::ePhysicalDeviceProperties2;
//        props2.pNext = &rtProps_;
//        device_.physical().getProperties2(&props2);
//        core::log::debug("RT Shader Group Handle Size: {}", rtProps_.shaderGroupHandleSize);
//    }
//
//

//
//    inline vk::UniquePipeline RayTracingPipeline::createRayTracingPipelineKHRUnique(
//            const vk::Device& device,
//            vk::PipelineCache pipelineCache,
//            const vk::RayTracingPipelineCreateInfoKHR& createInfo,
//            const vk::AllocationCallbacks* allocator = nullptr) {
//        vk::Pipeline pipeline;
//        auto result = device.createRayTracingPipelinesKHR(pipelineCache, 1, &createInfo, allocator, &pipeline);
//        if (result != vk::Result::eSuccess) {
//            throw std::runtime_error("Failed to create ray tracing pipeline");
//        }
//        return vk::UniquePipeline(pipeline, device, allocator);
//    }
//

//    inline void RayTracingPipeline::createShaderBindingTable() {
//        // create shader binding table here
//    }
//
//    inline void RayTracingPipeline::recordTraceRays(const vk::CommandBuffer& cmdBuf, vk::Extent2D extent) {
//        // Fill StridedDeviceAddressRegionKHR for rgen, miss, chit, etc.
//        // call traceRays and record them in a buffer
//    }


}