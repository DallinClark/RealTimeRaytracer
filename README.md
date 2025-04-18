# RealTimeRaytracer

**How to Set Up Local Dev Environment On Linux**
Download the Vulkan Sdk installer <a href="https://vulkan.lunarg.com/sdk/home" style="font-size: 48px;">Here</a>  
Create a work directory for Vulkan by running these commands, note: replace x.yy.z with actual version numbers, the standard for this project is 1.4.309.0
```
cd ~
mkdir Lib/vulkan
cd vulkan
```
Generate the sha256sum
```
sha256sum $HOME/Downloads/vulkansdk-linux-x86_64-1.x.yy.z.tar.xz
```
Extract the SDK package
```
tar xf $HOME/Downloads/vulkansdk-linux-x86_64-1.x.yy.z.tar.xz
```
To test if it is working run:
```
source ~/vulkan/1.x.yy.z/setup-env.sh
vkvia
```
Next we will install glfw, download the source file <a href="https://www.glfw.org/download.html" style="font-size: 48px;">Here</a>  
Open CMake and build into a file of your choice, but make sure to change the CMAKE_INSTALL_PREFIX into /glm  
Navigate to the file where you built and run
```
make
make install DESTDIR=$HOME/Lib
```
We're building into the ~/Lib for everything to keep it standard and per person, due to the environment used by developers  
Next download GLM <a href="https://github.com/g-truc/glm/releases" style="font-size: 48px;">Here</a>  
Navigate to the downloaded directory and run
```
mv glm ~/Lib
```
Now it should work with our CMake.txt file


