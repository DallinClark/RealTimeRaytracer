module;
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
export module RayTracingPipeline;

import VulkanContext;
export namespace vulkan {
    class RayTracingPipeline {
    public:
        vk::UniquePipelineLayout pipelineLayout;

    };
}
//
// import VulkanContext;
//
// export namespace vulkan {
//
// class RayTracingPipeline {
// public:
//     vk::UniquePipelineLayout pipelineLayout;
//     vk::UniqueHandle<vk::Pipeline, vk::detail::DispatchLoaderDynamic> pipeline;
//
//     RayTracingPipeline(
//         const VulkanContext&           context,
//         const vk::DescriptorSetLayout descriptorSetLayout,
//         const std::string_view        shaderPath
//     );
//
//     ~RayTracingPipeline() = default; // All destruction is automatic
// };
//
// vk::UniqueShaderModule loadShaderModule(
//     const vk::Device device,
//     std::string_view filename
// ) {
//     // Read the file into a uint32_t vector
//     std::ifstream file {filename.data(), std::ios::binary};
//     if (!file) throw std::runtime_error(std::format("Failed to open shader file ({})", filename));
//     const std::vector<uint32_t> code {
//         std::istreambuf_iterator<char>(file),
//         std::istreambuf_iterator<char>()
//     };
//
//     vk::ShaderModuleCreateInfo shaderModuleInfo {};
//     shaderModuleInfo.codeSize = code.size() * sizeof(uint32_t);
//     shaderModuleInfo.pCode    = code.data();
//
//     return device.createShaderModuleUnique(shaderModuleInfo);
// }
//
// RayTracingPipeline::RayTracingPipeline(
//     const VulkanContext &context,
//     const vk::DescriptorSetLayout descriptorSetLayout,
//     const std::string_view shaderPath
// ) {
//     auto& device = context.device;
//
//     // Create pipeline layout with a single descriptor-set layout
//     vk::PipelineLayoutCreateInfo layoutInfo {};
//     layoutInfo.setLayoutCount = 1;
//     layoutInfo.pSetLayouts    = &descriptorSetLayout;
//     pipelineLayout = device->createPipelineLayoutUnique(layoutInfo);
//
//     // Load our shaders
//     const auto shaderPathString  = std::string(shaderPath.begin(), shaderPath.end());
//     auto raygenShaderModule      = loadShaderModule(*device, shaderPathString + "/raygen.rgen.spv");
//     auto missShaderModule        = loadShaderModule(*device, shaderPathString + "/miss.rmiss.spv");
//     auto closestHitShaderModule  = loadShaderModule(*device, shaderPathString + "/hit.rchit.spv");
//
//     // Wrap them in PipelineShaderStageCreateInfo
//     auto raygenStage = vk::PipelineShaderStageCreateInfo {};
//     raygenStage.stage  = vk::ShaderStageFlagBits::eRaygenKHR;
//     raygenStage.module = *raygenShaderModule;
//     raygenStage.pName  = "main";
//
//     auto missStage = vk::PipelineShaderStageCreateInfo {};
//     missStage.stage  = vk::ShaderStageFlagBits::eMissKHR;
//     missStage.module = *missShaderModule;
//     missStage.pName  = "main";
//
//     auto closestHitStage = vk::PipelineShaderStageCreateInfo {};
//     closestHitStage.stage  = vk::ShaderStageFlagBits::eClosestHitKHR;
//     closestHitStage.module = *closestHitShaderModule;
//     closestHitStage.pName  = "main";
//
//     std::array stages = {raygenStage, missStage, closestHitStage};
//
//     // Define shader groups (one per stage / hit group)
//     vk::RayTracingShaderGroupCreateInfoKHR raygenGroup {};
//     raygenGroup.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
//     raygenGroup.setGeneralShader(0);
//     raygenGroup.setClosestHitShader(VK_SHADER_UNUSED_KHR);
//     raygenGroup.setAnyHitShader(VK_SHADER_UNUSED_KHR);
//     raygenGroup.setIntersectionShader(VK_SHADER_UNUSED_KHR);
//
//     vk::RayTracingShaderGroupCreateInfoKHR missGroup {};
//     missGroup.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
//     missGroup.setGeneralShader(1);
//     missGroup.setClosestHitShader(VK_SHADER_UNUSED_KHR);
//     missGroup.setAnyHitShader(VK_SHADER_UNUSED_KHR);
//     missGroup.setIntersectionShader(VK_SHADER_UNUSED_KHR);
//
//     vk::RayTracingShaderGroupCreateInfoKHR hitGroup {};
//     hitGroup.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
//     hitGroup.setGeneralShader(VK_SHADER_UNUSED_KHR);
//     hitGroup.setClosestHitShader(2);
//     hitGroup.setAnyHitShader(VK_SHADER_UNUSED_KHR);
//     hitGroup.setIntersectionShader(VK_SHADER_UNUSED_KHR);
//
//     std::array groups = {raygenGroup, missGroup, hitGroup};
//
//
//     // Fill in the pipeline creation info
//     vk::RayTracingPipelineCreateInfoKHR pipelineInfo {};
//     pipelineInfo.stageCount                   = static_cast<uint32_t>(stages.size());
//     pipelineInfo.pStages                      = stages.data();
//     pipelineInfo.groupCount                   = static_cast<uint32_t>(groups.size());
//     pipelineInfo.pGroups                      = groups.data();
//     pipelineInfo.maxPipelineRayRecursionDepth = 2; // e.g. primaty + one bounce
//     pipelineInfo.layout                       = *pipelineLayout;
//
//     // Create the pipeline (no cache, no additional alloc callbacks, this may need to change)
//     auto result = device->createRayTracingPipelineKHRUnique(
//         {},        // DeferredOperationKHR = VK_NULL_HANDLE
//         {},        // vk::PipelineCache    = VK_NULL_HANDLE
//         pipelineInfo,
//         nullptr,   // vk::AllocationCallbacks = VK_NULL_HANDLE
//         VULKAN_HPP_DEFAULT_DISPATCHER
//     );
//     pipeline = std::move(result.value);
// }
//
// } // namespace vulkan