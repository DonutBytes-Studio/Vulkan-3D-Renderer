include(FetchContent)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_subdirectory(third_party/volk)
add_subdirectory(third_party/vma)
add_subdirectory(third_party/glm)
add_subdirectory(third_party/tiny_gltf)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/sdl)