import app;

#include <exception>
#include <cstdio>
#include <cstdlib>

int main() {
    try {
        app::Application application("Real Time RayTracer", 800, 600, /*enableValidation=*/true);
        application.run();
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
