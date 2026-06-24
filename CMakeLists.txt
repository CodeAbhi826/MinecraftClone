cmake_minimum_required(VERSION 3.20)
project(BlockClone LANGUAGES C CXX)

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
add_executable(blockclone ${SOURCES})
target_include_directories(blockclone PRIVATE src)
target_link_libraries(blockclone PRIVATE OpenGL::GL glfw glm::glm glad)
if(enet_FOUND)
    target_link_libraries(blockclone PRIVATE enet::enet)
    target_compile_definitions(blockclone PRIVATE HAS_ENET)
endif()
