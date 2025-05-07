import app;

#include <cstdlib>
#include <iostream>
#include <print>

int main() {
    try {
        app::Application application("Real Time RayTracer", 800, 600, /*enableValidation=*/true);
        application.run();
    }
    catch (const std::exception& e) {
        std::println(std::cerr, "Fatal: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
