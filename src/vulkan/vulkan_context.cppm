module;

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <exception>

#include <GLFW/glfw3.h>

export module VulkanContext;

export namespace vulkan  {

class VulkanContext {
public:
    vk::UniqueInstance   instance;
    vk::PhysicalDevice   physicalDevice;
    vk::UniqueDevice     device;
    vk::Queue            graphicsQueue;
    uint32_t             graphicsQueueFamily = 0;

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
    // load the Vulkan loader and initialize the default dispatcher with that loader
    vk::detail::DynamicLoader dyn;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dyn);

    createInstance(appName);

    // after instance exists, pull in instance‐level functions
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());

    pickPhysicalDevice();
    createLogicalDevice();

    // after device exists, pull in device‐level functions
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

    graphicsQueue = device->getQueue(graphicsQueueFamily, 0);
}

void VulkanContext::createInstance(std::string_view appName) {
    // Creates an appInfo struct used to give info to our Vulkan Instance
    vk::ApplicationInfo appInfo {};
    appInfo.sType = vk::StructureType::eApplicationInfo;
    appInfo.pApplicationName   = appName.data();
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName        = "NoEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // Creates an instance struct that wraps our appInfo and adds extension information and validation layers
    vk::InstanceCreateInfo createInfo {};
    createInfo.sType = vk::StructureType::eInstanceCreateInfo;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    createInfo.enabledLayerCount = 0;

    /* Todo: validation layers could be added here, for a tutorial go to
       https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Validation_layers,
       for now just get needed glfw extensions */

    try {
        instance = vk::createInstanceUnique(createInfo);
    }
    catch (vk::SystemError& err) {
        std::cerr
                << "Failed to create Vulkan instance: "
                << err.what()
                << " (Vulkan error " << err.code() << ")\n";
        std::terminate();
    }
}

void VulkanContext::pickPhysicalDevice() {
    try {
        for (auto &currDevice: instance->enumeratePhysicalDevices()) {
            if (isRayTracingCapable(currDevice)) {
                physicalDevice = currDevice;
                return;
            }
        }
        throw std::runtime_error("No GPU found with Vulkan ray tracing support");
    }
    catch (std::runtime_error& err) {
        std::cerr << err.what();
        std::terminate();
    }
}

bool VulkanContext::isRayTracingCapable(const vk::PhysicalDevice testDevice) noexcept {
    // Check extensions
    auto availableExtensions = testDevice.enumerateDeviceExtensionProperties();
    for (auto const* requiredExtension : RequiredExtensions) {
        // If no matching extension is found, bail out
        if (!std::any_of(
                availableExtensions.begin(), availableExtensions.end(),
                [&](auto const& extension) {
                    // Compare the device’s extensionName to our required string
                    return std::string_view{extension.extensionName} == requiredExtension;
                }))
        {
            return false;
        }
    }

    // Find graphics queue
    auto families = testDevice.getQueueFamilyProperties();
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