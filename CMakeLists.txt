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
option(CUBE_TRACY "Enable Tracy profiling" OFF)

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

if (CUBE_TRACY)
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
  set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    tracy
    URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.11.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(tracy)
endif()

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
  src/core/log.cpp
  src/core/log.hpp
  src/core/profile.hpp
  src/core/tracy_new_delete.cpp
  src/memory/leak.cpp
  src/memory/leak.hpp
  src/memory/allocator.hpp
  src/memory/linear_allocator.hpp
  src/memory/pool_allocator.hpp
  src/memory/stack_allocator.hpp
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
  src/render/gpu_memory.cpp
  src/render/gpu_memory.hpp
  src/render/gpu_uploader.cpp
  src/render/gpu_uploader.hpp
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
if (CUBE_TRACY)
  target_link_libraries(cube PRIVATE TracyClient)
  target_compile_definitions(cube PRIVATE TRACY_ENABLE)
endif()

target_include_directories(cube PRIVATE ${CMAKE_SOURCE_DIR}/src ${vma_SOURCE_DIR}/include)

add_executable(cube_tests
  tests/math_tests.cpp
  tests/memory_tests.cpp
)
target_link_libraries(cube_tests PRIVATE glm::glm)
target_include_directories(cube_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
