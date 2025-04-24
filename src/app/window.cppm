module;

#include <GLFW/glfw3.h>

#include <string>
#include <memory>


export module WindowManager;

export namespace app  {

class WindowManager {
public:
    WindowManager(std::string_view windowName, int windowWidth, int windowHeight);
    GLFWwindow* getWindow() { return window; }
    ~WindowManager();

private:
    GLFWwindow* window;
};

WindowManager::WindowManager(std::string_view windowName, const int windowWidth, const int windowHeight) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Disables resizing the window, on for now
    window = glfwCreateWindow(windowWidth, windowHeight, windowName.data(), nullptr, nullptr);

}

WindowManager::~WindowManager() {
    glfwDestroyWindow(window);
    glfwTerminate();
}


} // namespace app