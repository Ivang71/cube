cmake_minimum_required(VERSION 3.20)
project(cube VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if (MSVC)
  add_compile_options(/MP)
endif()

option(CUBE_UNITY_BUILD "Enable unity/jumbo build for faster compiles" ON)

include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.3.9.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glfw)

find_package(Vulkan REQUIRED)

# GLM (OpenGL Mathematics)
FetchContent_Declare(
  glm
  URL https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glm)

# VMA (Vulkan Memory Allocator)
FetchContent_Declare(
  vma
  URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.0.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(vma)

# Dear ImGui
FetchContent_Declare(
  imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw Vulkan::Vulkan)

# Compile shaders
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/shaders/triangle.vert -o ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/triangle.vert
    COMMENT "Compiling triangle.vert to SPIR-V"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
    COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/shaders/triangle.frag -o ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/triangle.frag
    COMMENT "Compiling triangle.frag to SPIR-V"
)

add_executable(cube
  src/core/app.cpp
  src/core/app.hpp
  src/core/console.cpp
  src/core/console.hpp
  src/render/vk_instance.cpp
  src/render/vk_instance.hpp
  src/render/vk_device.cpp
  src/render/vk_device.hpp
  src/render/swapchain.cpp
  src/render/swapchain.hpp
  src/render/frame.cpp
  src/render/frame.hpp
  src/render/shader.cpp
  src/render/shader.hpp
  src/render/render_pass.cpp
  src/render/render_pass.hpp
  src/render/pipeline.cpp
  src/render/pipeline.hpp
  src/render/imgui_layer.cpp
  src/render/imgui_layer.hpp
  src/render/types.hpp
  src/main.cpp
  ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
  ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
)

if (CUBE_UNITY_BUILD)
  set_target_properties(cube PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 8)
endif()

target_link_libraries(cube PRIVATE glfw Vulkan::Vulkan VulkanMemoryAllocator glm::glm)
target_link_libraries(cube PRIVATE imgui_lib)

target_include_directories(cube PRIVATE ${CMAKE_SOURCE_DIR}/src ${vma_SOURCE_DIR}/include)
