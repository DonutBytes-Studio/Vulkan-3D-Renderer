include(FetchContent)

# ============================== Vulkan ==============================
find_package(Vulkan REQUIRED COMPONENTS volk)

# ============================== Shaderc =============================
set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)

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
message(STATUS "Setting up Shaderc, this might take a while...")
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${SHADERC_SOURCE_DIR}/utils/git-sync-deps
    WORKING_DIRECTORY ${SHADERC_SOURCE_DIR}
    RESULT_VARIABLE SYNC_RESULT
)
if(NOT SYNC_RESULT EQUAL 0)
    message(FATAL_ERROR "git-sync-deps failed for shaderc")
endif()

add_subdirectory(${SHADERC_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps/shaderc-build)

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
if(NOT SDL3_FOUND)
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
