module;

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string_view>
#include <set>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <cstring>

export module vulkan.context.device;

import core.log;
import vulkan.context.instance;
import vulkan.dispatch;

namespace vulkan::context {

export class Device {
public:
    /// Pick a physical GPU and create a logical device with ray tracing support.
    /// Also retrieves one compute & one present queue.
    Device(Instance& instance,
           vk::SurfaceKHR surface,
           bool enableValidation = true);

    ~Device() = default;

    /// Raw vk::Device handle
    [[nodiscard]] const vk::Device& get() const noexcept { return *device_; }

    /// The physical device backing our logical device
    [[nodiscard]] vk::PhysicalDevice physical() const noexcept { return physicalDevice_; }

    /// Queue for recording ray tracing commands
    [[nodiscard]] vk::Queue computeQueue() const noexcept { return computeQueue_; }

    /// Queue for presenting swapchain images
    [[nodiscard]] vk::Queue presentQueue() const noexcept { return presentQueue_; }

    /// Family index for compute commands
    [[nodiscard]] uint32_t computeFamilyIndex() const noexcept { return computeFamilyIdx_; }

    /// Family index for presentation
    [[nodiscard]] uint32_t presentFamilyIndex() const noexcept { return presentFamilyIdx_; }

private:
    vk::UniqueDevice   device_;
    vk::PhysicalDevice physicalDevice_;
    uint32_t           computeFamilyIdx_{};
    uint32_t           presentFamilyIdx_{};
    vk::Queue          computeQueue_;
    vk::Queue          presentQueue_;

    // Required device extensions for swapchain + ray tracing
    static std::vector<const char*> requiredExtensions() noexcept {
        return {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };
    }

    // Check if this GPU supports all required extensions
    static bool checkExtensionSupport(const vk::PhysicalDevice pd) {
        auto available = pd.enumerateDeviceExtensionProperties();
        for (const auto required = requiredExtensions(); auto const* extension : required) {
            const bool found = std::ranges::any_of(
                available,
                [&](auto const& prop) {
                    return std::strcmp(prop.extensionName, extension)==0;
                }
            );
            if (!found) {
                core::log::debug("  ✖  missing extension '{}'", extension);
                return false;
            }
        }
        core::log::debug("  ✓  all required extensions present");
        return true;
    }

    // Indices of the queues we need
    struct QueueFamilies {
        std::optional<uint32_t> compute;
        std::optional<uint32_t> present;
        bool complete() const noexcept { return compute.has_value() && present.has_value(); }
    };

    // Find a compute queue and a present queue on this GPU
    static QueueFamilies findQueueFamilies(const vk::PhysicalDevice pd,
                                           const vk::SurfaceKHR surface)
    {
        QueueFamilies indices;
        const auto families = pd.getQueueFamilyProperties();
        for (uint32_t i = 0; i < families.size(); ++i) {
            if (!indices.compute.has_value()
                && (families[i].queueFlags & vk::QueueFlagBits::eCompute))
            {
                indices.compute = i;
            }
            if (!indices.present.has_value()
                && pd.getSurfaceSupportKHR(i, surface))
            {
                indices.present = i;
            }
            if (indices.complete()) break;
        }
        return indices;
    }

    // Choose the first GPU that supports extensions + required queues
    static vk::PhysicalDevice pickPhysical(const vk::Instance instance,
                                           const vk::SurfaceKHR surface)
    {
        const auto devices = instance.enumeratePhysicalDevices();
        core::log::trace("Found {} Vulkan‑capable GPU(s)", devices.size());
        if (devices.empty())
            throw std::runtime_error("No Vulkan-capable GPUs found");

        for (auto const& device : devices) {
            auto properties = device.getProperties();
            core::log::debug("Evaluating GPU '{}'", properties.deviceName.data());

            if (!checkExtensionSupport(device)) continue;

            if (auto q = findQueueFamilies(device, surface); q.complete()) {
                core::log::info("Selected GPU '{}'", properties.deviceName.data());
                return device;
            }
            core::log::debug("  ✖  required queue families not found");
        }
        core::log::error("No suitable GPU met all requirements");
        throw std::runtime_error("Failed to find a GPU with required features");
    }
};

inline Device::Device(
        Instance& instance,
        vk::SurfaceKHR surface,
        bool /*enableValidation*/
) {
    core::log::info("Creating logical device...");

    // Pick a suitable GPU
    physicalDevice_ = pickPhysical(instance.get(), surface);

    // Find queue family indices
    const auto [compute, present] = findQueueFamilies(physicalDevice_, surface);
    computeFamilyIdx_ = *compute;
    presentFamilyIdx_ = *present;
    core::log::debug("Using queue families → compute: {}, present: {}", computeFamilyIdx_, presentFamilyIdx_);

    // Create unique queue-create infos (avoid duplicates)
    std::set uniqueFamilies = {
        computeFamilyIdx_,
        presentFamilyIdx_
    };
    float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queues;
    for (auto idx : uniqueFamilies) {
        queues.push_back({
            {},              // flags
            idx,             // queueFamilyIndex
            1,               // queueCount
            &priority        // pQueuePriorities
        });
    }

    // Enable ray tracing & device-address features
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferFeatures {};
    bufferFeatures.bufferDeviceAddress = VK_TRUE;

    // Allows for descriptor indexing in shaders, might not be needed
    vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.descriptorBindingPartiallyBound          = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray                   = VK_TRUE;
    descriptorIndexingFeatures.pNext                                    = &bufferFeatures;

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures {};
    accelFeatures.accelerationStructure = VK_TRUE;
    accelFeatures.pNext                 = &descriptorIndexingFeatures;

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures {};
    rtFeatures.rayTracingPipeline = VK_TRUE;
    rtFeatures.pNext              = &accelFeatures;


    // — Keep these alive until after device creation —
    auto layerNames    = std::vector<const char*>{};
    auto extensionNames = requiredExtensions();
    vk::PhysicalDeviceFeatures deviceFeatures{};

    // Fill device-create info
    vk::DeviceCreateInfo info {};
    info.queueCreateInfoCount    = static_cast<uint32_t>(queues.size());
    info.pQueueCreateInfos       = queues.data();
    info.enabledLayerCount       = static_cast<uint32_t>(layerNames.size());
    info.ppEnabledLayerNames     = layerNames.data();
    info.enabledExtensionCount   = static_cast<uint32_t>(extensionNames.size());
    info.ppEnabledExtensionNames = extensionNames.data();
    info.pEnabledFeatures        = &deviceFeatures;
    info.pNext                   = &rtFeatures;

    // Create the logical device
    device_ = physicalDevice_.createDeviceUnique(info, nullptr);
    dispatch::init_device(device_.get());
    core::log::info("Logical device created");

    // Retrieve the queues
    computeQueue_ = device_->getQueue(computeFamilyIdx_, 0);
    presentQueue_ = device_->getQueue(presentFamilyIdx_, 0);
    core::log::trace("Queues retrieved successfully");
}

} // namespace vulkan::context
