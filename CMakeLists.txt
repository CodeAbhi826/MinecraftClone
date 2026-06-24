cmake_minimum_required(VERSION 3.20)
project(VoxelCraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(enet CONFIG QUIET)

# GLAD (OpenGL loader)
add_library(glad STATIC src/glad/glad.c)
target_include_directories(glad PUBLIC src/glad)

# Main executable
file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(voxelcraft ${SOURCES})
target_include_directories(voxelcraft PRIVATE src)
target_link_libraries(voxelcraft PRIVATE OpenGL::GL glfw glm::glm glad)
if(enet_FOUND)
    target_link_libraries(voxelcraft PRIVATE enet::enet)
    target_compile_definitions(voxelcraft PRIVATE HAS_ENET)
endif()
