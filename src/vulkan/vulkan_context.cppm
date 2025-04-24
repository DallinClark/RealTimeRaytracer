module;

#include <vector>
#include <string>
#include <stdexcept>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

export module VulkanContext;

export namespace vulkan  {

class VulkanContext {
public:
    vk::UniqueInstance   instance;
    vk::PhysicalDevice   physicalDevice;
    vk::UniqueDevice     device;
    vk::Queue            graphicsQueue;
    uint32_t             graphicsQueueFamily = 0;

    vk::detail::DispatchLoaderDynamic dldi;

    explicit VulkanContext(std::string_view appName = "RealTimeRaytracer");
    ~VulkanContext() = default;  // All destruction is automatic

private:
    static constexpr std::array<const char*, 4> RequiredExtensions{
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    void createInstance(std::string_view appName);
    void pickPhysicalDevice();
    void createLogicalDevice();

    bool isRayTracingCapable(vk::PhysicalDevice device) noexcept;
};

VulkanContext::VulkanContext(const std::string_view appName) {
    createInstance(appName);
    vk::detail::DynamicLoader dyn;
    dldi.init( instance.get(),
              dyn.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr") );

    pickPhysicalDevice();
    createLogicalDevice();
    dldi.init(device.get());

    graphicsQueue = device->getQueue(graphicsQueueFamily, 0);

}

void VulkanContext::createInstance(std::string_view appName) {
    vk::ApplicationInfo appInfo {};
    appInfo.pApplicationName   = appName.data();
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName        = "NoEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo instanceInfo {};
    instanceInfo.pApplicationInfo = &appInfo;
    // Todo: validation layers could be added here

    instance = vk::createInstanceUnique(instanceInfo);
}

void VulkanContext::pickPhysicalDevice() {
    for (auto& device : instance->enumeratePhysicalDevices()) {
        if (isRayTracingCapable(device)) {
            physicalDevice = device;
            return;
        }
    }
    throw std::runtime_error("No GPU found with Vulkan ray tracing support");
}

bool VulkanContext::isRayTracingCapable(const vk::PhysicalDevice device) noexcept {
    // Check extensions
    auto availibleExtensions = device.enumerateDeviceExtensionProperties();
    for (auto const* requiredExtension : RequiredExtensions) {
        // If no matching extension is found, bail out
        if (!std::any_of(
                availibleExtensions.begin(), availibleExtensions.end(),
                [&](auto const& extension) {
                    // Compare the device’s extensionName to our required string
                    return std::string_view{extension.extensionName} == requiredExtension;
                }))
        {
            return false;
        }
    }

    // Find graphics queue
    auto families = device.getQueueFamilyProperties();
    for (std::vector<vk::QueueFamilyProperties>::size_type i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphicsQueueFamily = i;
            return true;
        }
    }

    return false; // No graphics queue found
}

void VulkanContext::createLogicalDevice() {
    constexpr float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo {};
    queueInfo.queueFamilyIndex = graphicsQueueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;

    // Enable ray-tracing features in a pNext chain
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures {};
    rtFeatures.rayTracingPipeline    = VK_TRUE;

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR asFeatures {};
    asFeatures.accelerationStructure = VK_TRUE;
    asFeatures.pNext                 = &rtFeatures;

    vk::PhysicalDeviceBufferDeviceAddressFeatures bufAddrFeatures {};
    bufAddrFeatures.bufferDeviceAddress   = VK_TRUE;
    bufAddrFeatures.pNext                 = &asFeatures;

    vk::DeviceCreateInfo deviceInfo {};
    deviceInfo.pNext                   = &bufAddrFeatures;
    deviceInfo.queueCreateInfoCount    = 1;
    deviceInfo.pQueueCreateInfos       = &queueInfo;
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(RequiredExtensions.size());
    deviceInfo.ppEnabledExtensionNames = RequiredExtensions.data();

    device = physicalDevice.createDeviceUnique(deviceInfo);
}

} // namespace vulkan