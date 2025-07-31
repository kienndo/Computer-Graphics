cmake_minimum_required(VERSION 3.10)
project(CG_hospital C CXX)

enable_language(C)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# === Dependencies ===
if(APPLE OR UNIX)
    find_package(Vulkan REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    find_package(Threads REQUIRED)
endif()

include_directories(include)

set(SOURCES
        src/main.cpp
        src/Libs.cpp
)

add_executable(CG_hospital ${SOURCES})

if(APPLE OR UNIX)
    target_link_libraries(CG_hospital PRIVATE Vulkan::Vulkan glfw Threads::Threads)
endif()
