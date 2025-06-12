module;

#include <string>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

export module app.window;

import core.log;
import scene.camera;

export namespace app  {

    class Window {
    public:
        Window(std::string_view windowName, int windowWidth, int windowHeight, float mouseSensitivity);
        ~Window();

        bool shouldClose() { return glfwWindowShouldClose(window_); }
        void processInput(float deltaTime, float camSpeed);

        [[nodiscard]] GLFWwindow* get() const { return window_; }
        [[nodiscard]] float getTime() const { return static_cast<float>(glfwGetTime()); }

        void setCamera(scene::Camera* camera) {camera_ = camera;}

    private:
        static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

        GLFWwindow*    window_;
        scene::Camera* camera_{nullptr};

        const int width_;
        const int height_;

        bool firstMouse_ = true;
        double lastX_ = 0.0;
        double lastY_ = 0.0;
        float mouseSensitivity_ = 0.5f;
    };

    Window::Window(const std::string_view windowName, const int windowWidth, const int windowHeight, const float mouseSensitivity)
        : width_(windowWidth), height_(windowHeight), mouseSensitivity_(mouseSensitivity) {
        if (!glfwInit()) {
            core::log::error("Failed to initialise GLFW");
            throw std::runtime_error("GLFW initialization failed");
        }

        // Configure window for Vulkan with fixed size
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window_ = glfwCreateWindow(windowWidth, windowHeight, windowName.data(), nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            core::log::error("Failed to create GLFW window");
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetCursorPosCallback(window_, mouseCallback);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void Window::processInput(float deltaTime, float camSpeed) {
        // Closes Window on ESC
        if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_, true);
        }

        glm::vec3 camPosition = camera_->getPosition();
        bool camMoved = false;

        if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) {
            camPosition += (camera_->getForward() * camSpeed);
            camMoved = true;
        }
        if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) {
            camPosition -= (camera_->getForward() * camSpeed);
            camMoved = true;
        }
        if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) {
            camPosition -= (camera_->getRight() * camSpeed);
            camMoved = true;
        }
        if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
            camPosition += (camera_->getRight() * camSpeed);
            camMoved = true;
        }
        if (camMoved) {
            camera_->setPosition(camPosition);
        }
    }

    void Window::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (!win || !win->camera_) return;

        if (win->firstMouse_) {
            win->lastX_ = xpos;
            win->lastY_ = ypos;
            win->firstMouse_ = false;
        }

        auto xOffset = static_cast<float>(xpos - win->lastX_);
        auto yOffset = static_cast<float>(win->lastY_ - ypos);

        win->lastX_ = xpos;
        win->lastY_ = ypos;

        xOffset *= win->mouseSensitivity_;
        yOffset *= win->mouseSensitivity_;

        win->camera_->processMouseMovement(xOffset, yOffset);
    }


    Window::~Window() {
        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
}

} // namespace app