cmake_minimum_required(VERSION 3.10)
project(CG_hospital C CXX)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output binary directly in build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ========== Platform-specific Setup ==========
if(APPLE)
    find_package(Vulkan REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    find_package(Threads REQUIRED)

    message(STATUS "Vulkan found at ${Vulkan_INCLUDE_DIR}")

    # Compile sources
    file(GLOB_RECURSE SOURCES src/*.cpp)
    file(GLOB_RECURSE HEADERS include/*.h include/*.hpp)

    add_executable(CG_hospital ${SOURCES} ${HEADERS})

    # Includes
    target_include_directories(CG_hospital PRIVATE
            ${Vulkan_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/include
    )

    # Libraries
    target_link_libraries(CG_hospital PRIVATE Vulkan::Vulkan glfw Threads::Threads)

    # Shader compilation (simple copy if .spv already exists)
    file(GLOB SPIRV_SOURCE_FILES "${CMAKE_SOURCE_DIR}/shaders/*.spv")
    message(STATUS "Found SPIR-V files: ${SPIRV_SOURCE_FILES}")

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
    foreach(SPV_FILE ${SPIRV_SOURCE_FILES})
        file(COPY ${SPV_FILE} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
    endforeach()

    # Custom target for syncing shaders
    add_custom_target(CG_hospital_Shaders DEPENDS ${SPIRV_SOURCE_FILES})
    add_dependencies(CG_hospital CG_hospital_Shaders)

    # Copy asset folders to build dir
    file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
    file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})

elseif(WIN32)
    set(GLFW "C:/VulkanSDK/libs/glfw-3.4.bin.WIN64")
    set(GLM "C:/VulkanSDK/libs/glm")

    list(APPEND INCLUDE_DIRS "${GLFW}/include" ${GLM} headers)
    list(APPEND LINK_LIBS "${GLFW}/lib-mingw-w64/libglfw3.a")

    file(GLOB_RECURSE SOURCES src/*.cpp)
    file(GLOB_RECURSE HEADERS include/*.h include/*.hpp)

    add_executable(CG_hospital ${SOURCES} ${HEADERS})

    find_package(Vulkan REQUIRED)

    foreach(dir IN LISTS Vulkan_INCLUDE_DIR INCLUDE_DIRS)
        target_include_directories(CG_hospital PUBLIC ${dir})
    endforeach()

    foreach(lib IN LISTS Vulkan_LIBRARIES LINK_LIBS)
        target_link_libraries(CG_hospital ${lib})
    endforeach()

    target_include_directories(CG_hospital PRIVATE ${CMAKE_SOURCE_DIR}/include)

    file(GLOB SPIRV_SOURCE_FILES "${CMAKE_SOURCE_DIR}/shaders/*.spv")
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
    foreach(SPV_FILE ${SPIRV_SOURCE_FILES})
        file(COPY ${SPV_FILE} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
    endforeach()

    add_custom_target(CG_hospital_Shaders DEPENDS ${SPIRV_SOURCE_FILES})
    add_dependencies(CG_hospital CG_hospital_Shaders)

    file(COPY ${CMAKE_SOURCE_DIR}/assets/textures DESTINATION ${CMAKE_BINARY_DIR}/assets)
    file(COPY ${CMAKE_SOURCE_DIR}/assets/models DESTINATION ${CMAKE_BINARY_DIR}/assets)

elseif(UNIX)
    find_package(Vulkan REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)

    file(GLOB_RECURSE SOURCES src/*.cpp)
    file(GLOB_RECURSE HEADERS include/*.h include/*.hpp)

    add_executable(CG_hospital ${SOURCES} ${HEADERS})

    target_include_directories(CG_hospital PRIVATE
            ${Vulkan_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(CG_hospital PRIVATE Vulkan::Vulkan glfw)

    file(GLOB SPIRV_SOURCE_FILES "${CMAKE_SOURCE_DIR}/shaders/*.spv")
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
    foreach(SPV_FILE ${SPIRV_SOURCE_FILES})
        file(COPY ${SPV_FILE} DESTINATION ${CMAKE_BINARY_DIR}/shaders)
    endforeach()

    add_custom_target(CG_hospital_Shaders DEPENDS ${SPIRV_SOURCE_FILES})
    add_dependencies(CG_hospital CG_hospital_Shaders)

    file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
    file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()
