cmake_minimum_required(VERSION 3.21)

project(gltf-viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gltf-viewer src/Main.cpp
						   src/glad.cpp
						   src/tiny_gltf.cpp
)

find_package(glfw3 CONFIG REQUIRED)

target_include_directories(gltf-viewer PRIVATE include)
target_link_libraries(gltf-viewer PRIVATE glfw)