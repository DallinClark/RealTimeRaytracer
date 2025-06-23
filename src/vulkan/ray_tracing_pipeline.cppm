module;
#include <vector>
#include <fstream>
#include <optional>

import core.file;
import core.log;
import vulkan.context.device;
import vulkan.context.command_pool;
import vulkan.memory.buffer;
import vulkan.types;

export module vulkan.ray_tracing_pipeline;



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
        void createShaderBindingTable();
        void recordTraceRays(const vk::CommandBuffer& cmdBuf, vk::Extent3D extent);


        [[nodiscard]] vk::Pipeline get() const noexcept { return pipeline_.get(); }
        [[nodiscard]] vk::PipelineLayout getLayout() const noexcept { return pipelineLayout_.get(); }

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
//        vk::UniqueBuffer sbtBuffer_;
//        vk::UniqueDeviceMemory sbtMemory_;

        // Ray tracing props
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{};

        // Internal utils
        inline vk::UniqueShaderModule createShaderModule(std::string_view path);
        void queryRayTracingProperties();

        vk::StridedDeviceAddressRegionKHR raygenRegion_;
        vk::StridedDeviceAddressRegionKHR missRegion_;
        vk::StridedDeviceAddressRegionKHR hitRegion_;

        std::optional<vulkan::memory::Buffer> raygenSBT_;
        std::optional<vulkan::memory::Buffer> missSBT_;
        std::optional<vulkan::memory::Buffer> hitSBT_;

    };


    RayTracingPipeline::RayTracingPipeline(const context::Device &device,
                                           std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts,
                                           std::string_view rgenPath, std::string_view rmissPath,
                                           std::string_view rchitPath) : device_{device}, descriptorSetLayouts_(descriptorSetLayouts) {
        createShaderModules(rgenPath, rmissPath, rchitPath);
        createShaderGroups();
        createPipelineLayout();
        createPipeline();
        createShaderBindingTable();
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
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           0,                   vk::ShaderUnusedKHR, vk::ShaderUnusedKHR, vk::ShaderUnusedKHR},
                {vk::RayTracingShaderGroupTypeKHR::eGeneral,           1,                   vk::ShaderUnusedKHR, vk::ShaderUnusedKHR, vk::ShaderUnusedKHR},
                {vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, vk::ShaderUnusedKHR, 2,                   vk::ShaderUnusedKHR, vk::ShaderUnusedKHR}
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


    void RayTracingPipeline::queryRayTracingProperties() {
        rtProps_.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
        vk::PhysicalDeviceProperties2 props2{};
        props2.sType = vk::StructureType::ePhysicalDeviceProperties2;
        props2.pNext = &rtProps_;
        device_.physical().getProperties2(&props2);
        core::log::debug("RT Shader Group Handle Size: {}", rtProps_.shaderGroupHandleSize);
    }

    void RayTracingPipeline::createShaderBindingTable() {
        queryRayTracingProperties();

        // Calculate shader binding table (SBT) size THIS MAY BE WRONG
        const uint32_t handleSize = rtProps_.shaderGroupHandleSize;
        const uint32_t handleSizeAligned = rtProps_.shaderGroupHandleAlignment;
        const uint32_t groupCount = static_cast<uint32_t>(shaderGroups_.size());
        const uint32_t sbtSize = handleSizeAligned * groupCount;

        // Get shader group handles
        std::vector<uint8_t> handleStorage(sbtSize);
        auto result = device_.get().getRayTracingShaderGroupHandlesKHR(pipeline_.get(), 0, groupCount, sbtSize, handleStorage.data());
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to get shader group handles");
        }

        // Create SBT
        raygenSBT_ = vulkan::memory::Buffer{device_, handleSize, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
        missSBT_   = vulkan::memory::Buffer{device_, handleSize, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
        hitSBT_    = vulkan::memory::Buffer{device_, handleSize, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

        raygenSBT_->fill(handleStorage.data() + 0 * handleSizeAligned, handleSize);
        missSBT_->fill(handleStorage.data()   + 1 * handleSizeAligned, handleSize);
        hitSBT_->fill(handleStorage.data()    + 2 * handleSizeAligned, handleSize);

        uint32_t stride = handleSizeAligned;
        uint32_t size = handleSizeAligned;

        raygenRegion_ = vk::StridedDeviceAddressRegionKHR{ raygenSBT_->getAddress(), stride, size };
        missRegion_   = vk::StridedDeviceAddressRegionKHR{ missSBT_->getAddress(),   stride, size };
        hitRegion_    = vk::StridedDeviceAddressRegionKHR{ hitSBT_->getAddress(),    stride, size };
    }

    inline void RayTracingPipeline::recordTraceRays(const vk::CommandBuffer& cmdBuf, vk::Extent3D extent) {
        cmdBuf.traceRaysKHR(raygenRegion_, missRegion_, hitRegion_, {}, extent.width, extent.height, 1);
    }


}