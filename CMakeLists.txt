cmake_minimum_required(VERSION 3.28.0)
project(firstsoloproj VERSION 0.1.0)
cmake_policy(SET CMP0072 NEW)
find_package(OpenGL REQUIRED)

add_executable(firstsoloproj src/main.cpp src/glad.h src/glad.c src/stb_image.h)

# ------------------------------------------------------------------
# >> FIX: Add the source directory to the include path for the target
target_include_directories(firstsoloproj PUBLIC src)
# ------------------------------------------------------------------

target_link_libraries(firstsoloproj glfw OpenGL::GL assimp)