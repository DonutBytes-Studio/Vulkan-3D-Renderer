include(FetchContent)
include(CheckCXXSourceCompiles)

option(FORCE_STATIC "Force static linking for fetched dependencies when possible" OFF)

# ============================== Vulkan / Shaderc ==============================
option(FORCE_BUILD_SHADERC "Always build Shaderc from source, even if a system version is available" OFF)

find_package(Vulkan REQUIRED COMPONENTS volk)
find_package(Vulkan QUIET COMPONENTS shaderc_combined)

set(SHADERC_TARGET "")

if(Vulkan_shaderc_combined_FOUND AND NOT FORCE_BUILD_SHADERC)
    # The SDK found a shaderc_combined lib, but on some Linux distro packages
    # it's NOT actually self-contained (missing glslang/SPIRV-Tools symbols).
    # Verify it really links before trusting it.
    set(CMAKE_REQUIRED_LIBRARIES Vulkan::shaderc_combined)
    check_cxx_source_compiles("
        #include <shaderc/shaderc.h>
        int main() {
            shaderc_compiler_t c = shaderc_compiler_initialize();
            shaderc_compiler_release(c);
            return 0;
        }
    " SHADERC_SDK_LINKS_OK)
    unset(CMAKE_REQUIRED_LIBRARIES)

    if(SHADERC_SDK_LINKS_OK)
        message(STATUS "Using shaderc_combined from Vulkan SDK")
        set(SHADERC_TARGET Vulkan::shaderc_combined)
    else()
        message(STATUS "SDK's shaderc_combined found but doesn't link cleanly (likely missing glslang/SPIRV-Tools deps) — will build from source instead")
    endif()
endif()

if(NOT SHADERC_TARGET)
    message(STATUS "Building shaderc from source (this can take a while)...")

    set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    # SHADERC_SKIP_INSTALL intentionally NOT set — see google/shaderc#1435

    set(SHADERC_SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/shaderc-src)

    if(NOT EXISTS ${SHADERC_SOURCE_DIR}/.git)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch v2024.4
                    https://github.com/google/shaderc.git ${SHADERC_SOURCE_DIR}
            RESULT_VARIABLE CLONE_RESULT
        )
        if(NOT CLONE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clone shaderc")
        endif()
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${SHADERC_SOURCE_DIR}/utils/git-sync-deps
        WORKING_DIRECTORY ${SHADERC_SOURCE_DIR}
        RESULT_VARIABLE SYNC_RESULT
    )
    if(NOT SYNC_RESULT EQUAL 0)
        message(FATAL_ERROR "git-sync-deps failed for shaderc")
    endif()

    add_subdirectory(${SHADERC_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps/shaderc-build)
    set(SHADERC_TARGET shaderc_combined)
endif()

# ============================== VMA ================================
find_package(VulkanMemoryAllocator 3.4.0 QUIET)
if(NOT VulkanMemoryAllocator_FOUND)
    FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.4.0
    )
    FetchContent_GetProperties(VulkanMemoryAllocator)
    if(NOT VulkanMemoryAllocator_POPULATED)
        FetchContent_MakeAvailable(VulkanMemoryAllocator)
    endif()
endif()

# ============================== SDL3 ===============================
find_package(SDL3 3.4.14 QUIET CONFIG)

if(TARGET SDL3::SDL3-static)
    set(SDL3_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3 AND NOT FORCE_STATIC)
    set(SDL3_TARGET SDL3::SDL3)
endif()

if(NOT SDL3_FOUND OR (FORCE_STATIC AND NOT TARGET SDL3::SDL3-static))
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.14
    )
    FetchContent_GetProperties(SDL3)
    if(NOT SDL3_POPULATED)
        FetchContent_MakeAvailable(SDL3)
    endif()
endif()

# ============================== GLM ================================
find_package(glm 1.0.3 QUIET)
if(NOT glm_FOUND)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
    )
    FetchContent_GetProperties(glm)
    if(NOT glm_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_MakeAvailable(glm)
        set_target_properties(glm PROPERTIES FOLDER "Dependencies")
    endif()
endif()
