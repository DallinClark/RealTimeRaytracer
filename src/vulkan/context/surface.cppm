module;

#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>
#include <stdexcept>

import vulkan.context.instance;

export module vulkan.context.surface;

namespace vulkan::context {

    /// Lightweight wrapper for a VkSurfaceKHR created via GLFW.
    export class Surface {
    public:
        /// Create a Vulkan surface for the given GLFW window.
        /// Throws if creation fails.
        explicit Surface(Instance& instance, GLFWwindow* window)
          : instance_(instance)
        {
            VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
            if (glfwCreateWindowSurface(
                    instance_.get(),
                    window,
                    nullptr,
                    &rawSurface
                ) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Vulkan surface");
            }

            surface_ = rawSurface;
        }

        /// Destroy the Vulkan surface on cleanup.
        ~Surface() {
            if (surface_ != VK_NULL_HANDLE) {
                instance_.get().destroySurfaceKHR(surface_);
            }
        }

        /// Retrieve the raw VkSurfaceKHR handle.
        [[nodiscard]] vk::SurfaceKHR get() const noexcept {
            return surface_;
        }

    private:
        Instance&        instance_;
        vk::SurfaceKHR   surface_{ VK_NULL_HANDLE };
    };

} // namespace vulkan::context
