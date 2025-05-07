module;

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

export module vulkan.dispatch;

namespace vulkan::dispatch {

inline vk::detail::DynamicLoader loader{};

export inline void init_loader() {
    const auto vkGetInstanceProcAddr = loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
}

export inline void init_instance(const vk::Instance instance) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

export inline void init_device(const vk::Device device) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
}

} // namespace vulkan::dispatch