# Building osgEarth from Scratch

Normally it is sufficient to install osgEarth from `vcpkg` and use it in your appplication. However, if you want to contribute to the project or make local modifications, you will need to build osgEarth yourself.

The documentation here is focused on Windows. 

## Building with vcpkg

[vcpkg](https://github.com/Microsoft/vcpkg) is a package manager. It works on Windows, Linux and MacOS but for this guide we will focus on Windows.

**Step 1 - Configure vcpkg**

First, clone and bootstrap [vcpkg](https://github.com/Microsoft/vcpkg) following the instructions on the page.

**Step 2 - (Optional) Configure the osg_OPENGL_PROFILE triplet variable**

You can configure OSG to build in GL3 or GLCORE mode if you require it. Some platforms (like OSX or VMWare) require a GLCORE build.

As of this writing, vcpkg will build OSG with OPENGL_PROFILE=GL2 by default.  This is sufficient for running many osgEarth applications, but if you run into trouble with newer features, try a GL3 or GLCORE build instead.

Open your existing x64-windows.cmake triplet file at path\to\vcpkg\triplets\x64-windows.cmake and add this line to the end of the file.
```
set(osg_OPENGL_PROFILE GL3)
```

This will build osg with modern OpenGL features and remove the fixed function pipeline.

If you wish to build OSG with a different OPENGL_PROFILE such as GLCORE you can set the osg_OPENGL_PROFILE to GLCORE like this
```
set(osg_OPENGL_PROFILE GLCORE)
```
When you install osg using vcpkg with this variable set it will build osg against the <GL/glcorearb.h> headers instead of the usual <GL/gl.h> header.

**Step 3 - Clone the repository**

Pull down the source from GitHub and create a ```build``` folder for your out-of-source build. We always recommend doing an out-of-source build to avoid problems down the road!

```
git clone --recurse-submodules https://github.com/gwaldron/osgearth.git osgearth
mkdir build
```

This will clone the repository into a folder called `osgearth` and pull down all the submodules.

**Step 4 - Configure CMake**

On Windows, you can run the provided script `bootstrap-vcpkg.bat` to configure your CMake build. This can take a while since it needs to download and build all your dependencies.

Otherwise, follow these steps:

vcpkg provides a CMake toolchain file that helps osgEarth find all of its dependencies.

Note: You’ll need to specify a different build directory based on your build configuration (Release, RelWIthDebInfo, Debug) and specify the build type using ```-DCMAKE_BUILD_TYPE```. This is because some dependencies of osgEarth don’t pick up both debug and release versions without specifying the build type. Hopefully this will be fixed in future CMake versions.

Most developers will use a RelWithDebInfo build, like so:

```
cmake -S osgearth -B build -G "Visual Studio 17 2022 Win64" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWIN32_USE_MP=ON -DCMAKE_INSTALL_PREFIX=[installroot] -DCMAKE_TOOLCHAIN_FILE=[vcpkgroot]\scripts\buildsystems\vcpkg.cmake
```

osgEarth provides a vcpkg.json manifest file that lists all of it's necessary dependencies.  The vcpkg toolchain integration will notice this file and install the necessary dependencies in your build\vcpkg_installed directory.

**Step 5 - Build and install osgEarth**

You can build and install osgEarth on the command line using CMake or you can open up the Visual Studio solution and build it from there.

```
cmake --build build --target INSTALL --config RelWithDebInfo
```

**Step 6 - Set up your runtime environment**

You’ll need to make sure that the vcpkg dependencies and osgEarth are in your path:

```
set PATH=%PATH%;path\to\build\vcpkg_installed\x64-windows\bin
set PATH=%PATH%;path\to\build\vcpkg_installed\x64-windows\tools\osg
set PATH=%PATH%;[installroot]
```

## Building with support for cesium-native
See documentation at [here](cesium_native.md) for building osgEarth with support for cesium-native.

## Checking for an OpenGL Core Profile Context
Some situations require you to have an OpenGL Core Profile context.  The ability to create a core context is available when OSG is built with OPENGL_PROFILE=GL3 or GLCORE.  Environments such as Apple OSX and VMWare require it as does debugging with tools like NVidia NSight.  You can check to see if you are running with an OpenGL Core Profile by running a command like this (Windows)
```
set OSG_GL_CONTEXT_VERSION=4.6
osgearth_version --caps
```

If all went well, it should report **"Core Profile = yes"**.

You can disable the CORE profile and select a compatibility profile by setting a profile mask like so

```
set OSG_GL_CONTEXT_PROFILE_MASK=1
```

The context version and profile mask are also settable via the `osg::DisplaySettings` class in the OpenSceneGraph API.

## Linux Build Example
vcpkg is one option for building osgEarth and all the dependencies, but it is also possible to use the Linux binary repositories to install dependencies quickly.  This is based on Ubuntu but the idea is the same, install the required dependencies and compile osgEarth.  Here are the basics:

Install Build essentials
```
sudo apt update && sudo apt install build-essential
```
Install GDAL:
```
sudo apt-get install libgdal-dev
```
Install GLEW
```
sudo apt-get install libglew-dev
```
Build Openscenegraph.  This sets GL profile and context which aren't necessary so change accordingly for your needs.
```
git clone https://github.com/openscenegraph/OpenSceneGraph.git
cd OpenSceneGraph
mkdir build && cd build
cmake .. -DOPENGL_PROFILE=GL3 -DOSG_GL_CONTEXT_VERSION=4.6
make -j8
sudo make install
```
Build Draco.  This is optional but included in case it is not available in a repo.  The -fPIC flag may be required on some platforms but not others.
```
git clone https://github.com/google/draco.git
cd draco
mkdir build && cd build
cmake .. -DCMAKE_CXX_FLAGS=-fPIC
make -j8
sudo make install
```
Build osgEarth. Turning off OSGEARTH_ENABLE_FASTDXT may not be necessary but left it here for platforms where it will not build
```
git clone https://github.com/gwaldron/osgearth.git
cd osgEarth
mkdir build && cd build
cmake .. -DOSGEARTH_ENABLE_FASTDXT=OFF
make -j8
sudo make install
```
After a successful build, it might be necessary to set your dynamic library search path to find both OpenScenegraph and osgEarth libraries.  Check path to osgPlugins folders also.
```
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
```
## Tips for Running on Windows WSL2 with nVidia GPU
In WSL2 (I have tried Ubuntu 20 and 22), follow the previous Linux build example with this change to Openscenegraph:
Build Openscenegraph.  This sets GL profile to Core and context to 3.3 for Mesa compatibility
```
git clone https://github.com/openscenegraph/OpenSceneGraph.git
cd OpenSceneGraph
mkdir build && cd build
cmake .. -DOPENGL_PROFILE=GL3 -DOSG_GL_CONTEXT_VERSION=3.3
make -j8
sudo make install
```
At that point, you should have all the osgEarth binaries built and installed.
Next follow this guide: [https://canonical-ubuntu-wsl.readthedocs-hosted.com/en/latest/tutorials/gpu-cuda/](https://canonical-ubuntu-wsl.readthedocs-hosted.com/en/latest/tutorials/gpu-cuda/#install-nvidia-cuda-on-ubuntu)

Shortcut steps but see the above link for more information.
```
sudo apt-key del 7fa2af80
wget https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/cuda-wsl-ubuntu.pin
sudo mv cuda-wsl-ubuntu.pin /etc/apt/preferences.d/cuda-repository-pin-600
sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/3bf863cc.pub
sudo add-apt-repository 'deb https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/ /'
sudo apt-get update
sudo apt-get -y install cuda
```
If you are on Ubuntu 23.10, there is a CUDA install problem.  This is the fix:

The libtinfo5 package isn't available in Ubuntu 23.10's default repositories yet. We can install it by adding the universe repo for Ubuntu 23.04 (Lunar Lobster).

Open a terminal window and run:
```
sudo nano /etc/apt/sources.list
```
Add this line (adds the Ubuntu 23.04 aka "Lunar Lobster" universe repository to apt):
```
deb http://archive.ubuntu.com/ubuntu/ lunar universe
```
Save and exit, then run:
```
sudo apt update
```
...and now the install command for CUDA should work, automatically downloading and installing libtinfo5 while installing CUDA

I did clone the CUDA samples repo and build deviceQuery which will be a quick test to make sure your GPU is recognized
```
git clone https://github.com/nvidia/cuda-samples
cd cuda-samples/Samples/1_Utilities/deviceQuery
make
./deviceQuery
```
If that successfully recognizes your nVidia GPU, you can try:
```
osgearth_version --caps

[osgEarth]  Hello, world.
[osgEarth]  [Registry] Note: GDAL_DATA environment variable is not set
[osgEarth]  [Capabilities] osgEarth Version:  3.5.0 build 149
[osgEarth]  [Capabilities] GDAL Version:      3.4.1
[osgEarth]  [Capabilities] OSG Version:       3.7.0
[osgEarth]  [Capabilities] OSG GL3 Features:  yes
[osgEarth]  [Capabilities] OSG FFP Available: no
[osgEarth]  [Capabilities] CPU Cores:         16
[osgEarth]  [Capabilities] GL_VENDOR:         Microsoft Corporation
[osgEarth]  [Capabilities] GL_RENDERER:       D3D12 (NVIDIA GeForce RTX 3070 Ti)
[osgEarth]  [Capabilities] GL_VERSION:        4.2 (Core Profile) Mesa 23.0.4-0ubuntu1~22.04.1
[osgEarth]  [Capabilities] GL CORE Profile:   yes
```
## Tips for VMware Users

Running osgEarth in a virtual machine environment can be tricky since they usually don't have direct access to the graphics hardware by default. If you are having trouble you can try these tips.

First, build OSG and osgEarth for GL CORE profile (as above). 

Next, assess the situation with a capabilities check:

```
osgearth_version --caps
```

The output will look something like this:

```
GPU Vendor:        WMware, Inc.
GPU Renderer       Gallium 0.3 on llvmpipe
GL/Driver Version: 1.2 Mesa 11.2.0
```

If is reports a Mesa driver, and the version is less than 3.3, you will need to configure a couple environment variables to move forward (Windows):

```
set OSG_GL_CONTEXT_VERSION=3.3
set MESA_GL_VERSION_OVERRIDE=3.3
osgearth_version --caps
```

Good luck!
