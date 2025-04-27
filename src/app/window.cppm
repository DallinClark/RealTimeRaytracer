module;

#include <string>
#include <stdexcept>
#include <GLFW/glfw3.h>

export module WindowManager;

export namespace app  {

class WindowManager
{
public:
    WindowManager(std::string_view windowName, int windowWidth, int windowHeight);
    GLFWwindow* getWindow() const { return window; }
    ~WindowManager();

private:
    GLFWwindow* window;
};

WindowManager::WindowManager(std::string_view windowName, const int windowWidth, const int windowHeight) {
    if (!glfwInit()) {
        throw std::runtime_error("GLFW initialization failed");
    }

    // Configure window for Vulkan with fixed size
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Disables resizing the window, on for now

    window = glfwCreateWindow(windowWidth, windowHeight, windowName.data(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

}

WindowManager::~WindowManager() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

} // namespace app