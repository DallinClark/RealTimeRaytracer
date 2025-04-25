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
    vk::UniqueSwapchainKHR swapChain;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    vk::Format swapChainImageFormat;
    vk::Extent2D swapChainExtent;
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
    void createSwapChain(GLFWwindow* window);
    void createImageViews();
    bool isDeviceCompatible(vk::PhysicalDevice device) noexcept;
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
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

    createSwapChain(window);
    createImageViews();
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

void VulkanContext::createSwapChain(GLFWwindow* window) {
    // First, find properties of swap chain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    const std::vector<vk::SurfaceFormatKHR> availableFormats = swapChainSupport.formats;
    vk::SurfaceFormatKHR surfaceFormat = availableFormats[0];

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb  &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surfaceFormat = availableFormat;
            continue;
        }
    }

    std::vector<vk::PresentModeKHR> availableModes = swapChainSupport.presentModes;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

    for (const auto& availableMode : availableModes) {
        if (availableMode == vk::PresentModeKHR::eMailbox) {
            presentMode = availableMode;
        }
    }

    vk::SurfaceCapabilitiesKHR surfaceCapabilities = swapChainSupport.capabilities;
    VkExtent2D extent;
    if ( surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() ) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, surfaceCapabilities.minImageExtent.height,
                                         surfaceCapabilities.maxImageExtent.height);

        extent = actualExtent;
    }
    else {
        // If the surface size is defined, the swap chain size must match
        extent = surfaceCapabilities.currentExtent;
    }

    vk::CompositeAlphaFlagBitsKHR compositeAlpha =
            ( surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied )
            ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            : ( surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied )
              ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
              : ( surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit )
                ? vk::CompositeAlphaFlagBitsKHR::eInherit
                : vk::CompositeAlphaFlagBitsKHR::eOpaque;

    // Actually making the swap chain
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.sType = vk::StructureType::eSwapchainCreateInfoKHR;
    createInfo.surface = surface.get();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // This may need to get changed for denoising, not sure

    if (graphicsQueueFamily != presentQueueFamily) {
        std::array<uint32_t,2> queueFamilies = { graphicsQueueFamily, presentQueueFamily };
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Specifies a transform to every image
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // doesn't worry about pixels that are obscured
    createInfo.oldSwapchain = VK_NULL_HANDLE; // might need to change to allow for resizing window
    swapChain = device->createSwapchainKHRUnique( createInfo );

    swapChainImages = device->getSwapchainImagesKHR( swapChain.get() );
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanContext::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.sType = vk::StructureType::eImageViewCreateInfo;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = swapChainImageFormat;

        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;

        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits ::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        swapChainImageViews[i] = device->createImageView( createInfo );
    }
}

void VulkanContext::pickPhysicalDevice() {
    for (auto &currDevice: instance->enumeratePhysicalDevices()) {
        if (isDeviceCompatible(currDevice)) {
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

bool VulkanContext::isDeviceCompatible(const vk::PhysicalDevice testDevice) noexcept {
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
    if (g == UINT32_MAX || p == UINT32_MAX) {
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