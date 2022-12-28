cmake_minimum_required(VERSION 3.21)

project(gltf-viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gltf-viewer 
src/Main.cpp
src/Animation.cpp
src/Animation.h
src/Camera.h
src/Entity.h
src/GLTFHelpers.h
src/GLTFHelpers.cpp
src/GLTFResources.cpp
src/GLTFResources.h
src/Input.h
src/Mesh.cpp
src/Mesh.h
src/PBRMaterial.h
src/Scene.h
src/Scene.cpp
src/Shader.cpp
src/Shader.h
src/Skeleton.h
src/Transform.h
src/Transform.cpp
src/VertexAttribute.h
src/glad.cpp
src/tiny_gltf.cpp
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_include_directories(gltf-viewer PRIVATE include)
target_link_libraries(gltf-viewer PRIVATE glfw glm::glm imgui::imgui)