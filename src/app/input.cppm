module;

#include <GLFW/glfw3.h>
#include <array>
#include <glm/vec2.hpp>

export module input;


namespace input {
    // —— Configuration ——
    constexpr int KeyCount = GLFW_KEY_LAST + 1;
    constexpr int ButtonCount = GLFW_MOUSE_BUTTON_LAST + 1;

    // —— Internal state ——
    namespace {
        std::array<bool, KeyCount>    keys{};
        std::array<bool, KeyCount>    prevKeys{};
        std::array<bool, ButtonCount> buttons{};
        std::array<bool, ButtonCount> prevButtons{};
        double mouseX  = 0.0, mouseY  = 0.0;
        double scrollX = 0.0, scrollY = 0.0;

        inline constexpr bool valid(const int idx, const int count) noexcept {
            return idx >= 0 && idx < count;
        }
    }

    // —— Initialization ——
    export void Init(GLFWwindow* window) noexcept {
        glfwSetKeyCallback(window, [](GLFWwindow*, const int key, int, const int action, int) {
            if (valid(key, KeyCount)) keys[key] = (action != GLFW_RELEASE);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow*, int b, const int action, int) {
            if (valid(b, ButtonCount)) buttons[b] = (action != GLFW_RELEASE);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow*, const double x, const double y) {
            mouseX = x;
            mouseY = y;
        });
        glfwSetScrollCallback(window, [](GLFWwindow*, const double x, const double y) {
            scrollX += x;
            scrollY += y;
        });
    }

    // —— Frame update ——
    export void PollEvents() noexcept {
        prevKeys    = keys;
        prevButtons = buttons;
        scrollX = scrollY = 0.0;
        glfwPollEvents();
    }

    // —— Key queries ——
    export constexpr bool IsKeyDown(const int key) noexcept {
        return valid(key, KeyCount) && keys[key];
    }
    export constexpr bool IsKeyJustPressed(const int key) noexcept {
        return valid(key, KeyCount) && keys[key] && !prevKeys[key];
    }
    export constexpr bool IsKeyJustReleased(const int key) noexcept {
        return valid(key, KeyCount) && !keys[key] && prevKeys[key];
    }

    // —— Mouse-button queries ——
    export constexpr bool IsMouseButtonDown(const int btn) noexcept {
        return valid(btn, ButtonCount) && buttons[btn];
    }
    export constexpr bool IsMouseButtonJustPressed(const int btn) noexcept {
        return valid(btn, ButtonCount) && buttons[btn] && !prevButtons[btn];
    }
    export constexpr bool IsMouseButtonJustReleased(const int btn) noexcept {
        return valid(btn, ButtonCount) && !buttons[btn] && prevButtons[btn];
    }

    // —— Mouse & scroll ——
    export glm::vec2 MousePosition() noexcept {
        return { mouseX, mouseY };
    }
    export glm::vec2 ScrollOffset() noexcept {
        return { scrollX, scrollY };
    }
}
