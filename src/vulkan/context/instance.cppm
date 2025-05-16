module;

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <vector>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>

export module vulkan.context.instance;

import core.log;
import vulkan.dispatch;

namespace vulkan::context {

export class Instance {
public:
    /// Construct a Vulkan instance with an application name.
    /// If enableValidation is true, also create a debug messenger.
    explicit Instance(std::string_view appName, bool enableValidation = true);

    /// Default destructor: vk::Unique* members clean up automatically
    ~Instance() = default;

    /// Raw vk::Instance handle
    [[nodiscard]] const vk::Instance& get() const noexcept {
        return *instance_;
    }

    /// Raw vk::DebugUtilsMessengerEXT handle (only valid if validation enabled)
    [[nodiscard]] const vk::DebugUtilsMessengerEXT& debugMessenger() const noexcept {
        return *debugMessenger_;
    }

    /// Was validation enabled at construction?
    [[nodiscard]] bool validationEnabled() const noexcept {
        return enableValidation_;
    }

private:
    vk::UniqueInstance               instance_;
    vk::UniqueDebugUtilsMessengerEXT debugMessenger_;
    bool                             enableValidation_;

    /// Which validation layers we want
    static std::vector<const char*> getValidationLayers() noexcept {
        return { "VK_LAYER_KHRONOS_validation" };
    }

    /// GLFW + (optionally) debug utils
    static std::vector<const char*> getRequiredExtensions(const bool enableValidation) {
        uint32_t count = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector extensions(glfwExtensions, glfwExtensions + count);
        if (enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    /// Check that all requested validation layers are supported
    static bool checkValidationLayerSupport(const std::vector<const char*>& layers) {
        auto available = vk::enumerateInstanceLayerProperties();
        for (auto const* layer : layers) {
            const bool found = std::ranges::any_of(
                available,
                [&](auto const& prop) {
                    return std::strcmp(prop.layerName, layer) == 0;
                }
            );
            if (!found) {
                core::log::debug("validation layer '{}' not present", layer);
                return false;
            }
        }
        core::log::debug("all required validation layers present");
        return true;
    }

    /// Create the debug messenger (Unique) using vk::Instance
    static vk::UniqueDebugUtilsMessengerEXT createDebugMessenger(const vk::Instance& instance) {
        vk::DebugUtilsMessengerCreateInfoEXT info{};
        info.messageSeverity =
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        info.messageType =
              vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
            | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        info.pfnUserCallback = debugCallback;
        return instance.createDebugUtilsMessengerEXTUnique(info);
    }

    /// Callback invoked by the Vulkan validation layer
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
        vk::DebugUtilsMessageTypeFlagsEXT             /*type*/,
        const vk::DebugUtilsMessengerCallbackDataEXT* data,
        void*                                         /*userData*/)
    {
        using S = vk::DebugUtilsMessageSeverityFlagBitsEXT;
        if (severity & S::eError)
            core::log::error("[VULKAN] {}", data->pMessage);
        else if (severity & S::eWarning)
            core::log::warn ("[VULKAN] {}", data->pMessage);
        else
            core::log::info ("[VULKAN] {}", data->pMessage);
        return VK_FALSE;
    }
};

// ——— Implementation —————————————————————————————————————————————

inline Instance::Instance(std::string_view appName, const bool enableValidation)
    : enableValidation_(enableValidation)
{
    core::log::info ("Creating Vulkan instance '{}'...", appName);

    // Verify validation layers if requested
    if (enableValidation_) {
        core::log::debug("  Validation layers requested");
        if (const auto layers = getValidationLayers(); !checkValidationLayerSupport(layers)) {
            core::log::error("Requested validation layers are unavailable");
            throw std::runtime_error("Validation layers requested but unavailable");
        }
    }

    // Application info
    const vk::ApplicationInfo appInfo {
        appName.data(),         // pApplicationName
        VK_MAKE_VERSION(1,0,0), // applicationVersion
        "NoEngine",             // pEngineName
        VK_MAKE_VERSION(1,0,0), // engineVersion
        VK_API_VERSION_1_2      // apiVersion
    };

    // Instance creation
    const auto extensions = getRequiredExtensions(enableValidation_);
    const auto layers     = enableValidation_ ? getValidationLayers() : std::vector<const char*>{};
    core::log::debug("  Using {} instance extension(s)", extensions.size());

    const vk::InstanceCreateInfo createInfo(
        {},                                       // InstanceCreateFlags
        &appInfo,                                 // pApplicationInfo
        static_cast<uint32_t>(layers.size()),     // enabledLayerCount
        layers.data(),                            // ppEnabledLayerNames
        static_cast<uint32_t>(extensions.size()), // enabledExtensionCount
        extensions.data()                         // ppEnabledExtensionNames
    );


    // Create the Vulkan instance
    instance_ = vk::createInstanceUnique(createInfo);
    vulkan::dispatch::init_instance(instance_.get());
    core::log::info ("Vulkan instance created");

    // Set up debug messenger if needed
    if (enableValidation_) {
        debugMessenger_ = createDebugMessenger(instance_.get());
        core::log::debug("Debug messenger set up");
    }
}

} // namespace vulkan::context
