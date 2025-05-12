# RealTimeRaytracer

**How to Set Up Local Dev Environment On Linux**

*Note: This should only be necessary if you are developing on a school managed machine.*

First, run `tools/bootstrap.sh`. This will do two things:

1. It will install system libraries in your local user space. They can be found at `~/.local`
2. It will put the path to the libraries in your `~/.bashrc` file.
3. It will download version 20.1 of LLVM under `~/tools`
4. It will clone and build version 3.3.10 of GLFW under `~/tools`
5. It will clone and build version 1.0.1 of GLM under `~/tools`
6. It will download version 1.4.309.0 of the Vulkan SDK under `~/tools`

Next, open the project in the IDE of your choice and enable the config presets found in `CMakePresets.json`.
For example, in CLion, go to `File -> Settings -> Build, Execution, Deployment -> CMake` and check the box for 
`Enable CMake Presets` on `llvm-local-debug` and `llvm-local-release`. Then, click `Apply` and `OK`.

Now, you need to set the path to the Vulkan SDK validation layers in the environment variable `VK_LAYER_PATH`. 
In CLion, go to `Run -> Edit Configurations -> Environment Variables` and add `VK_LAYER_PATH=PATH_TOHOME/tools/vulkan/1.4.309.0/x86_64/share/vulkan/explicit_layer.d`

Finally, run the project using the `llvm-local-debug` or `llvm-local-release` configuration.
