#include <cstdlib>
#include <format>
#include <print>
#include <vector>
#include <vulkan/vulkan.hpp>

static vk::Instance instance{};

//–– Helper to turn a Vulkan version int into a string ––
inline std::string version_string(const uint32_t v) {
    return std::format("{}.{}.{}",
        VK_VERSION_MAJOR(v),
        VK_VERSION_MINOR(v),
        VK_VERSION_PATCH(v)
    );
}

void createInstance() {
    std::println("Creating Vulkan instance…");

    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName   = "DeviceInfoApp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo        = &appInfo;

#ifdef __APPLE__
    // — macOS needs the portability enumeration extension. We can remove this once we require NVIDIA RTX —
    const char* extensions[] = { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
    createInfo.enabledExtensionCount   = 1;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.flags                   = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    instance = vk::createInstance(createInfo);
    std::println("✔ Instance created");
}

void listInstanceExtensions() {
    std::println("\nAvailable instance extensions:");
    auto extensionProperties = vk::enumerateInstanceExtensionProperties(nullptr);
    for (auto const& prop : extensionProperties) {
        std::string_view name(prop.extensionName);
        std::println("  • {} (v{})",
            name,
            version_string(prop.specVersion)
        );
    }
}

void listPhysicalDevices() {
    std::println("\nEnumerating physical devices...");
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        std::println("⚠ No Vulkan‐capable devices found");
        return;
    }

    for (auto device : devices) {
        auto properties = device.getProperties();

        std::string_view deviceNameView(properties.deviceName);
        std::println("\nDevice Name    : {}", deviceNameView);
        std::println("API Version    : {}", version_string(properties.apiVersion));
        std::println("Driver Version : {}", version_string(properties.driverVersion));
        std::println("Vendor ID      : {}    Device ID: {}",
                     properties.vendorID, properties.deviceID);

        const char* type_name = nullptr;
        switch (properties.deviceType) {
        case vk::PhysicalDeviceType::eIntegratedGpu: type_name = "Integrated GPU"; break;
        case vk::PhysicalDeviceType::eDiscreteGpu:   type_name = "Discrete GPU";   break;
        case vk::PhysicalDeviceType::eCpu:           type_name = "CPU";            break;
        case vk::PhysicalDeviceType::eVirtualGpu:    type_name = "Virtual GPU";    break;
        default:                                     type_name = "Other";          break;
        }
        std::println("Device Type    : {}", type_name);
    }
}

void cleanup() {
    if (instance) {
        instance.destroy();
        instance = vk::Instance{};
    }
}

int main() {
    try {
        createInstance();
        listInstanceExtensions();
        listPhysicalDevices();
        cleanup();
    }
    catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        cleanup();
        return EXIT_FAILURE;
    }

    std::println("\n✔ All done!");
    return EXIT_SUCCESS;
}
