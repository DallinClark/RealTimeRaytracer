module;
#include <vector>
#include <string>
#include <fstream>
#include <vulkan/vulkan.hpp>

export module vulkan.compute_pipeline;

import core.file;
import core.log;
import vulkan.context.device;

namespace vulkan {

    export class ComputePipeline {
    public:
        ComputePipeline(const context::Device &device,
                        const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                        std::string_view computeShaderPath) : device_(device), descriptorSetLayouts_(descriptorSetLayouts) {
            createShaderModule(computeShaderPath);
            createPipelineLayout();
            createPipeline();
        }

        [[nodiscard]] vk::Pipeline get() const noexcept { return pipeline_.get(); }
        [[nodiscard]] vk::PipelineLayout getLayout() const noexcept { return pipelineLayout_.get(); }

    private:
        const context::Device& device_;
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts_{};

        vk::UniqueShaderModule computeShader_;
        vk::UniquePipelineLayout pipelineLayout_;
        vk::UniquePipeline pipeline_;

        void createShaderModule(std::string_view path) {
            std::vector<char> code = core::file::loadBinaryFile(path);
            vk::ShaderModuleCreateInfo createInfo{{}, code.size(), reinterpret_cast<const uint32_t*>(code.data())};
            computeShader_ = device_.get().createShaderModuleUnique(createInfo);
        }

        void createPipelineLayout() {
            vk::PipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts_.size());
            layoutInfo.pSetLayouts = descriptorSetLayouts_.data();

            // Push Constant is just an int to tell which image
            vk::PushConstantRange pushConstantRange{};
            pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eCompute);
            pushConstantRange.setOffset(0);
            pushConstantRange.setSize(sizeof(int));

            layoutInfo.setPushConstantRangeCount(1);
            layoutInfo.setPPushConstantRanges(&pushConstantRange);

            pipelineLayout_ = device_.get().createPipelineLayoutUnique(layoutInfo);
        }

        void createPipeline() {
            vk::PipelineShaderStageCreateInfo stageInfo{};
            stageInfo.setStage(vk::ShaderStageFlagBits::eCompute);
            stageInfo.setModule(computeShader_.get());
            stageInfo.setPName("main");

            vk::ComputePipelineCreateInfo pipelineInfo{};
            pipelineInfo.setStage(stageInfo);
            pipelineInfo.setLayout(pipelineLayout_.get());

            pipeline_ = device_.get().createComputePipelineUnique(nullptr, pipelineInfo).value;
        }
    };

}
