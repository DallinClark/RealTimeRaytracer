module;
#include <vector>
#include <string>
#include <fstream>
#include <vulkan/vulkan.hpp>
#include <optional>

export module vulkan.ray_tracing_pipeline;

import core.file;
import core.log;
import vulkan.context.device;
import vulkan.context.command_pool;
import vulkan.memory.buffer;

import scene.scene_info;

// TODO redo this class to be better at creating sbts
namespace vulkan {

    export class RayTracingPipeline {
    public:
        RayTracingPipeline(const context::Device &device, std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                           std::string_view rgenPath, std::string_view rmissPath,
                           std::string_view shadowMissPath, std::string_view rchitPath);

        void createShaderModules(std::string_view rgen, std::string_view rmiss, std::string_view shadowMiss, std::string_view rchit);

        void createShaderGroups();

        void createPipelineLayout();

        void createPipeline();
        void createShaderBindingTable();
        void recordTraceRays(const vk::CommandBuffer& cmdBuf, vk::Extent3D extent);


        [[nodiscard]] vk::Pipeline get() const noexcept { return pipeline_.get(); }
        [[nodiscard]] vk::PipelineLayout getLayout() const noexcept { return pipelineLayout_.get(); }

    private:
        const context::Device &device_;
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts_;

        // Shader modules
        vk::UniqueShaderModule rgen_, rmiss_, shadowMiss_, rchit_;
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups_;
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages_;

        // Pipeline
        vk::UniquePipelineLayout pipelineLayout_;
        vk::UniquePipeline pipeline_;

        // Ray tracing props
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{};

        // Internal utils
        inline vk::UniqueShaderModule createShaderModule(std::string_view path);
        void queryRayTracingProperties();

        vk::StridedDeviceAddressRegionKHR raygenRegion_ = {};
        vk::StridedDeviceAddressRegionKHR missRegion_ = {};
        vk::StridedDeviceAddressRegionKHR hitRegion_ = {};


        std::optional<vulkan::memory::Buffer> SBTbuffer_;


    };


    RayTracingPipeline::RayTracingPipeline(const context::Device &device, std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                                   std::string_view rgenPath, std::string_view rmissPath,
                                   std::string_view shadowMissPath, std::string_view rchitPath)
                                   : device_{device}, descriptorSetLayouts_(descriptorSetLayouts) {

        createShaderModules(rgenPath, rmissPath, shadowMissPath, rchitPath);
        createShaderGroups();
        createPipelineLayout();
        createPipeline();
        createShaderBindingTable();
    }

    void RayTracingPipeline::createShaderModules(std::string_view rgen, std::string_view rmiss, std::string_view shadowMiss, std::string_view rchit) {
        rgen_ = createShaderModule(rgen);
        rmiss_ = createShaderModule(rmiss);
        shadowMiss_ =  createShaderModule(shadowMiss);
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
                {{}, vk::ShaderStageFlagBits::eRaygenKHR,     rgen_.get(),  "main"}, // main is the entry point
                {{}, vk::ShaderStageFlagBits::eMissKHR,       rmiss_.get(), "main"},
                {{}, vk::ShaderStageFlagBits::eMissKHR,       shadowMiss_.get(), "main"},
                {{}, vk::ShaderStageFlagBits::eClosestHitKHR, rchit_.get(), "main"},
        };
        shaderGroups_ = {
                // 0: raygen
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
                // 1: main miss
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
                // 2: shadow miss
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
                // 3: main hit group
                {vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
        };
    }

    void RayTracingPipeline::createPipelineLayout() {
        vk::PipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.setLayoutCount = descriptorSetLayouts_.size();
        layoutInfo.pSetLayouts = &descriptorSetLayouts_[0];

        // For now, just add one push constant that is a SceneInfo struct
        vk::PushConstantRange pushConstant;
        pushConstant.setOffset(0);
        pushConstant.setSize(sizeof(scene::SceneInfo));
        pushConstant.setStageFlags(vk::ShaderStageFlagBits::eClosestHitKHR);

        layoutInfo.setPPushConstantRanges(&pushConstant);
        layoutInfo.setPushConstantRangeCount(1);

        pipelineLayout_ = device_.get().createPipelineLayoutUnique(layoutInfo);
    }

    void RayTracingPipeline::createPipeline() {

        vk::RayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages_.size());
        pipelineInfo.pStages = shaderStages_.data();
        pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups_.size());
        pipelineInfo.pGroups = shaderGroups_.data();
        pipelineInfo.layout = pipelineLayout_.get();
        pipelineInfo.maxPipelineRayRecursionDepth = 4;

        pipeline_ = device_.get().createRayTracingPipelineKHRUnique({}, {}, pipelineInfo).value;
    }
    // Implemented functions


    void RayTracingPipeline::queryRayTracingProperties() {
        rtProps_.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
        vk::PhysicalDeviceProperties2 props2{};
        props2.sType = vk::StructureType::ePhysicalDeviceProperties2;
        props2.pNext = &rtProps_;
        device_.physical().getProperties2(&props2);
        core::log::debug("RT Shader Group Handle Size: {}", rtProps_.shaderGroupHandleSize);
    }

    template<typename T, typename U>
    inline T align_up(T value, U alignment) {
        return (value + static_cast<T>(alignment) - 1) & ~(static_cast<T>(alignment) - 1);
    }


    void RayTracingPipeline::createShaderBindingTable() {
        queryRayTracingProperties();

        uint32_t missCount{2};
        uint32_t hitCount{1};
        auto handleCount = 1 + missCount + hitCount;
        const uint32_t handleSize = rtProps_.shaderGroupHandleSize;
        const uint32_t handleSizeAligned = align_up(handleSize, rtProps_.shaderGroupHandleAlignment);

        auto shaderGroupBaseAlignment = rtProps_.shaderGroupBaseAlignment;

        raygenRegion_.stride = align_up(handleSizeAligned, shaderGroupBaseAlignment);
        raygenRegion_.size   = raygenRegion_.stride;
        missRegion_.stride = handleSizeAligned;
        missRegion_.size   = align_up(missCount * handleSizeAligned, shaderGroupBaseAlignment);
        hitRegion_.stride  = handleSizeAligned;
        hitRegion_.size    = align_up(hitCount * handleSizeAligned, shaderGroupBaseAlignment);

        // Get shader group handles
        uint32_t dataSize = handleCount * handleSize;
        std::vector<uint8_t> handles(dataSize);
        auto result = device_.get().getRayTracingShaderGroupHandlesKHR(pipeline_.get(), 0, handleCount, dataSize, handles.data());
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to get shader group handles");
        }

        // Create SBT
        vk::DeviceSize sbtSize = raygenRegion_.size + missRegion_.size + hitRegion_.size;
        SBTbuffer_ = vulkan::memory::Buffer{device_, sbtSize, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferSrc , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

        // FUTURE IMPLEMENT THIS m_debug.setObjectName(m_rtSBTBuffer.buffer, std::string("SBT"));  // Give it a debug name for NSight.
        vk::DeviceAddress sbtAddress = SBTbuffer_->getAddress();
        raygenRegion_.deviceAddress = sbtAddress;
        missRegion_.deviceAddress   = sbtAddress + raygenRegion_.size;
        hitRegion_.deviceAddress    = sbtAddress + raygenRegion_.size + missRegion_.size;

        // TODO this can be better all of it
        std::vector<vulkan::memory::Buffer::FillRegion> regions = {
                {handles.data() + handleSize * 0, handleSize, 0},
                {handles.data() + handleSize * 1, handleSize, raygenRegion_.size},
                {handles.data() + handleSize * 2, handleSize, raygenRegion_.size + missRegion_.stride},
                {handles.data() + handleSize * 3, handleSize, raygenRegion_.size + missRegion_.size}
        };

        SBTbuffer_->fill(regions);
    }

    inline void RayTracingPipeline::recordTraceRays(const vk::CommandBuffer& cmdBuf, vk::Extent3D extent) {
        cmdBuf.traceRaysKHR(raygenRegion_, missRegion_, hitRegion_, {}, extent.width, extent.height, 1);
    }


}