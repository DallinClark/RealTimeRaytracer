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
    vk::UniqueInstance         instance;
    vk::PhysicalDevice         physicalDevice;
    vk::UniqueDevice           device;
    vk::UniqueSurfaceKHR       surface;
    vk::UniqueSwapchainKHR     swapChain;
    std::vector<vk::Image>     swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    vk::Format                 swapChainImageFormat;
    vk::Extent2D               swapChainExtent;
    vk::Queue                  graphicsQueue;
    vk::Queue                  presentQueue;
    uint32_t                   graphicsQueueFamily = UINT32_MAX;
    uint32_t                   presentQueueFamily  = UINT32_MAX;

    explicit VulkanContext(std::string_view appName = "RealTimeRaytracer", GLFWwindow* window = nullptr);
    ~VulkanContext() = default;  // All destruction is automatic

private:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR        capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR>   presentModes;
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
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
};

VulkanContext::VulkanContext(const std::string_view appName, GLFWwindow* window) {
    // Load the Vulkan loader and initialize dispatcher
    const vk::detail::DynamicLoader loader;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(loader);

    createInstance(appName);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());

    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());

    createSwapChain(window);
    createImageViews();
}

void VulkanContext::createInstance(const std::string_view appName) {
    // Application info
    vk::ApplicationInfo appInfo {};
    appInfo.pApplicationName   = appName.data();
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName        = "NoEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Creates an instance struct that wraps our appInfo and adds extension information and validation layers
    vk::InstanceCreateInfo createInfo {};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    /* Todo: validation layers could be added here, for a tutorial go to
       https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Validation_layers,
       for now just get needed glfw extensions */

    try
    {
        instance = vk::createInstanceUnique(createInfo);
    } catch (vk::SystemError& err) {
        std::println(std::cerr, "Failed to create Vulkan instance: {} (Vulkan error {})", err.what(), err.code().value());
        std::terminate();
    }
}

void VulkanContext::createSurface(GLFWwindow* window) {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(vk::Instance(instance.get()), window, nullptr, &rawSurface ) != VK_SUCCESS )
        throw std::runtime_error( "Failed to create window!" );

    const vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> _deleter( instance.get() );
    surface = vk::UniqueSurfaceKHR( vk::SurfaceKHR( rawSurface ), _deleter );
}

void VulkanContext::pickPhysicalDevice() {
    for (const auto &currDevice: instance->enumeratePhysicalDevices()) {
        if (isDeviceCompatible(currDevice)) {
            physicalDevice = currDevice;
            return;
        }
    }
    throw std::runtime_error("No GPU found with Vulkan ray tracing support");
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



VulkanContext::SwapChainSupportDetails VulkanContext::querySwapChainSupport(const vk::PhysicalDevice device) {
    return {
        device.getSurfaceCapabilitiesKHR(surface.get()),
        device.getSurfaceFormatsKHR(surface.get()),
        device.getSurfacePresentModesKHR(surface.get())
    };
}

bool VulkanContext::isDeviceCompatible(const vk::PhysicalDevice device) noexcept {
    // Check extensions
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    for (auto const* requiredExtension : RequiredExtensions) {
        // If no matching extension is found, bail out
        if (!std::ranges::any_of(
            availableExtensions,
            [&](auto const& extension) {
                // Compare the device’s extensionName to our required string
                return std::string_view{extension.extensionName} == requiredExtension;
            })
        ) {
            return false;
        }
    }

    if (const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainSupport.formats.empty() ||
            swapChainSupport.presentModes.empty()) {
        return false;
    }


    const auto queueFamilies = device.getQueueFamilyProperties();
    std::optional<uint32_t> graphicsQueueFamily;
    std::optional<uint32_t> presentQueueFamily;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& family = queueFamilies[i];

        // First time we see a graphics queue, record it
        if (!graphicsQueueFamily && (family.queueFlags & vk::QueueFlagBits::eGraphics)) {
            graphicsQueueFamily = i;
        }

        // First time we see a present queue, record it
        if (!presentQueueFamily && device.getSurfaceSupportKHR(i, surface.get())) {
            presentQueueFamily = i;
        }

        // As soon as we have both, no need to keep looping
        if (graphicsQueueFamily && presentQueueFamily){
            break;
        }
    }

    return graphicsQueueFamily.has_value() && presentQueueFamily.has_value();
}

void VulkanContext::createSwapChain(GLFWwindow* window) {
    // First, find properties of swap chain
    auto [capabilities, formats, presentModes] = querySwapChainSupport(physicalDevice);

    // Choose format
    const auto formatIterator = std::ranges::find_if(
        formats,
        [](auto const& f){
            return f.format == vk::Format::eB8G8R8A8Srgb &&
            f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    );
    const auto surfaceFormat = formatIterator != formats.end() ? *formatIterator : formats.front();

    // Choose present mode
    const auto modeIt = std::ranges::find(
        presentModes,
        vk::PresentModeKHR::eMailbox
    );
    const auto presentMode = modeIt != presentModes.end() ? *modeIt : vk::PresentModeKHR::eFifo;

    // Determine extent
    const auto surfaceCapabilities = capabilities;
    const vk::Extent2D extent = surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()
        ? surfaceCapabilities.currentExtent
        : vk::Extent2D{
            std::clamp<uint32_t>(
                static_cast<uint32_t>([window] { int w,h; glfwGetFramebufferSize(window,&w,&h); return w; }()),
                surfaceCapabilities.minImageExtent.width,
                surfaceCapabilities.maxImageExtent.width
            ),
            std::clamp<uint32_t>(
                static_cast<uint32_t>([window] { int w,h; glfwGetFramebufferSize(window,&w,&h); return h; }()),
                surfaceCapabilities.minImageExtent.height,
                surfaceCapabilities.maxImageExtent.height)
        };

    // Composite alpha
    auto alphaFlags = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    for (const auto flag : {vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                            vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
                            vk::CompositeAlphaFlagBitsKHR::eInherit})
    {
        if (surfaceCapabilities.supportedCompositeAlpha & flag) { alphaFlags = flag; break; }
    }

    // Actually making the swap chain
    const uint32_t imageCount = std::clamp(
        surfaceCapabilities.minImageCount + 1,
        surfaceCapabilities.minImageCount,
        surfaceCapabilities.maxImageCount > 0 ?
            surfaceCapabilities.maxImageCount :
            surfaceCapabilities.minImageCount + 1
    );

    vk::SwapchainCreateInfoKHR swapchainInfo {};
    swapchainInfo.surface          = surface.get();
    swapchainInfo.minImageCount    = imageCount;
    swapchainInfo.imageFormat      = surfaceFormat.format;
    swapchainInfo.imageColorSpace  = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent      = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment; // This may need to change for denoising

    if (graphicsQueueFamily != presentQueueFamily) {
        const std::array queueFamilies = { graphicsQueueFamily, presentQueueFamily };
        swapchainInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices   = queueFamilies.data();
    } else {
        swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    swapchainInfo.preTransform   = surfaceCapabilities.currentTransform; // Specifies a transform to every image
    swapchainInfo.compositeAlpha = alphaFlags;
    swapchainInfo.presentMode    = presentMode;
    swapchainInfo.clipped        = VK_TRUE; // doesn't worry about pixels that are obscured
    swapchainInfo.oldSwapchain   = VK_NULL_HANDLE; // might need to change to allow for resizing window

    swapChain = device->createSwapchainKHRUnique(swapchainInfo);
    swapChainImages = device->getSwapchainImagesKHR(swapChain.get());
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanContext::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.image    = swapChainImages[i];
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format   = swapChainImageFormat;

        imageViewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits ::eColor;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;

        swapChainImageViews[i] = device->createImageView(imageViewInfo);
    }
}

} // namespace vulkan