cmake_minimum_required(VERSION 3.21)

project(gltf-viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gltf-viewer src/Main.cpp
                           src/json.hpp
                           src/stb_image.h
                           src/stb_image_write.h
                           src/tiny_gltf.cpp
                           src/tiny_gltf.h
)