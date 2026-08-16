include(FetchContent)

# shaderc comes from the Vulkan SDK component (Vulkan::shaderc_combined) on
# both platforms. Windows: install the LunarG Vulkan SDK. Arch: `pacman -S
# shaderc vulkan-headers vulkan-icd-loader`. Requires CMake >= 3.24 for the
# shaderc_combined component to be recognized by FindVulkan.
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_subdirectory(third_party/volk)
add_subdirectory(third_party/vma)
add_subdirectory(third_party/glm)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/sdl)