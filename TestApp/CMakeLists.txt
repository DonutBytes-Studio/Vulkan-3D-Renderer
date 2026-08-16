cmake_minimum_required(VERSION 4.3.1)

#Set this to true for shipping!
set(PRODUCTION_BUILD FALSE CACHE BOOL "Make this a production build" FORCE)

project(TestApp)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(TestApp ${MY_SOURCES})

if(MSVC)
    target_compile_options(TestApp PRIVATE /std:c++latest)
endif()

if(PRODUCTION_BUILD)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RESOURCES_PATH="./resources/") #Relative path
    target_compile_definitions(${PROJECT_NAME} PUBLIC PRODUCTION_BUILD=1)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/") #Absolute path... ONLY DURING DEVELOPEMENT!
    target_compile_definitions(${PROJECT_NAME} PUBLIC PRODUCTION_BUILD=0)
endif()

include(Dependencies.cmake)

target_link_libraries(TestApp PRIVATE
    volk
    VulkanMemoryAllocator
    Vulkan::shaderc_combined
    SDL3-static
    glm
)

target_compile_definitions(TestApp PUBLIC VK_NO_PROTOTYPES)
target_include_directories(TestApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/)
