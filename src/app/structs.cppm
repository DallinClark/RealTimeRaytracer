module;

#include <string>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

export module app.structs;

import core.log;
import scene.camera;

export namespace app {
    struct DenoisingInfo {
        int step_width;
        float c_phi;
        float n_phi;
        float p_phi;
        int denoisedImagesAreOutput; // use 0 or 1
        int isShadowedImage;         // use 0 or 1
    };
}
