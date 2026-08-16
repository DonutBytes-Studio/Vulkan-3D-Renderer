# Vulkan 3D Renderer

## Installation

Firstly

```bash
git clone --recurse-submodules -j8 https://github.com/DonutBytes-Studio/Vulkan-3D-Renderer.git
```

Then build with Cmake.

### Windows

Download and setup the Vulkan SDK, make sure to download the Shader Toolchain with it.

Turn on the validation layers.

### Linux

You'll need Vulkan to work of course (drivers, etc...)
If you can, install the Vulkan SDK, otherwise :

#### Arch

Install these packages :

- `vulkan-headers`
- `vulkan-icd-loader`
- `shaderc`
