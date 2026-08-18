cmake_minimum_required(VERSION 4.3.1)

project(tiny_gltf)

add_library(tiny_gltf)
target_include_directories(tiny_gltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(tiny_gltf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tiny_gltf.cpp)