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
    vk::UniqueSurfaceKHR surface;
    vk::Queue            graphicsQueue;
    vk::Queue            presentQueue;
    uint32_t             graphicsQueueFamily = UINT32_MAX;
    uint32_t             presentQueueFamily  = UINT32_MAX;

    explicit VulkanContext(std::string_view appName = "RealTimeRaytracer", GLFWwindow* window = nullptr);
    ~VulkanContext() = default;  // All destruction is automatic

private:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    static constexpr std::array<const char*, 5> RequiredExtensions{
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    void createInstance(std::string_view appName);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface(GLFWwindow* window);
    bool isRayTracingCapable(vk::PhysicalDevice device) noexcept;
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice testDevice);
};

VulkanContext::VulkanContext(const std::string_view appName, GLFWwindow* window) {
    // load the Vulkan loader and initialize the default dispatcher with that loader
    vk::detail::DynamicLoader dyn;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dyn);

    createInstance(appName);

    // after instance exists, pull in instance‐level functions
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());
    createSurface(window);

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

void VulkanContext::createSurface(GLFWwindow* window) {
    VkSurfaceKHR _surface;
    VkResult err = glfwCreateWindowSurface( VkInstance( instance.get() ), window, nullptr, &_surface );
    if ( err != VK_SUCCESS )
        throw std::runtime_error( "Failed to create window!" );

    vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> _deleter( instance.get() );
    surface = vk::UniqueSurfaceKHR( vk::SurfaceKHR( _surface ), _deleter );
}

void VulkanContext::pickPhysicalDevice() {
    for (auto &currDevice: instance->enumeratePhysicalDevices()) {
        if (isRayTracingCapable(currDevice)) {
            physicalDevice = currDevice;
            return;
        }
    }
    throw std::runtime_error("No GPU found with Vulkan ray tracing support");
}

VulkanContext::SwapChainSupportDetails VulkanContext::querySwapChainSupport(vk::PhysicalDevice testDevice) {
    SwapChainSupportDetails details;
    details.capabilities = testDevice.getSurfaceCapabilitiesKHR(surface.get());
    details.formats     = testDevice.getSurfaceFormatsKHR(      surface.get());
    details.presentModes = testDevice.getSurfacePresentModesKHR(surface.get());
    return details;
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

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(testDevice);
    bool swapChainAdequate =
            !swapChainSupport.formats.empty() &&
            !swapChainSupport.presentModes.empty();
    if (!swapChainAdequate) {
        return false;
    }


    // TODO clean this up
    auto families = testDevice.getQueueFamilyProperties();
    uint32_t g = UINT32_MAX, p = UINT32_MAX;
    for (uint32_t i = 0; i < families.size(); ++i) {
        bool isGraphics = static_cast<bool>(families[i].queueFlags & vk::QueueFlagBits::eGraphics);
        bool isPresent = testDevice.getSurfaceSupportKHR(i, surface.get());
        if (isGraphics) g = i;
        if (isPresent) p = i;
        if (isGraphics && isPresent) {
            return true;
        }
    }
    if (g != UINT32_MAX || p != UINT32_MAX) {
        return false;
    }
    return true;
}

void VulkanContext::createLogicalDevice() {
    constexpr float priority = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    vk::DeviceQueueCreateInfo gfxQueueInfo{};
    gfxQueueInfo.queueFamilyIndex = graphicsQueueFamily;
    gfxQueueInfo.queueCount       = 1;
    gfxQueueInfo.pQueuePriorities = &priority;
    queueInfos.push_back(gfxQueueInfo);

    if (presentQueueFamily != graphicsQueueFamily) {
        vk::DeviceQueueCreateInfo presQueueInfo{};
        presQueueInfo.queueFamilyIndex = presentQueueFamily;
        presQueueInfo.queueCount       = 1;
        presQueueInfo.pQueuePriorities = &priority;
        queueInfos.push_back(presQueueInfo);
    }

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
    deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos       = queueInfos.data();
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(RequiredExtensions.size());
    deviceInfo.ppEnabledExtensionNames = RequiredExtensions.data();

    device = physicalDevice.createDeviceUnique(deviceInfo);

    graphicsQueue = device->getQueue(graphicsQueueFamily, 0);
    presentQueue  = device->getQueue(presentQueueFamily,  0);
}

} // namespace vulkan