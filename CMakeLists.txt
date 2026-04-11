cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
endif()

project(
    electron
    LANGUAGES CXX
)

# CMake

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

option(BUILD_ELECTRON_EXAMPLE OFF)
option(BUILD_ELECTRON_TESTING OFF)
option(ELECTRON_USES_LOCAL_CPPFRONT OFF)
if(WIN32)
  set(ELECTRON_USES_SDL3 ON CACHE BOOL "")
else()
  set(ELECTRON_USES_SDL3 OFF CACHE BOOL "")
endif()

set(ELECTRON_SOURCE_DIR ${electron_SOURCE_DIR})

file(GLOB_RECURSE ELECTRON_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/src/*.cpp)
add_library(electron STATIC ${ELECTRON_SOURCE_FILES})
target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/include)
target_link_libraries(electron PUBLIC neutron)

set(ELECTRON_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/third_party)
set(ELECTRON_THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
add_subdirectory(${ELECTRON_THIRD_PARTY_DIR}/imgui)
add_subdirectory(${ELECTRON_THIRD_PARTY_SOURCE_DIR}/neutron)
add_subdirectory(${ELECTRON_THIRD_PARTY_SOURCE_DIR}/stdexec)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${ELECTRON_THIRD_PARTY_SOURCE_DIR}/glfw)
add_subdirectory(${ELECTRON_THIRD_PARTY_SOURCE_DIR}/vulkan-headers)
add_subdirectory(${ELECTRON_THIRD_PARTY_SOURCE_DIR}/VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)
target_link_libraries(electron PUBLIC
  imgui::imgui
  glfw
  Vulkan::Vulkan # for sdl & imgui impl
  Vulkan::Headers
  STDEXEC::stdexec
  GPUOpen::VulkanMemoryAllocator
)

target_compile_definitions(electron PUBLIC ELECTRON_GLFW_VULKAN_APP)

target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/third_party/imgui/api/vulkan/include)
file(GLOB_RECURSE ELECTRON_THIRD_PARTY_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/third_party/imgui/api/vulkan/src/*.cpp)
target_sources(electron PRIVATE ${ELECTRON_THIRD_PARTY_SOURCE_FILES})

add_subdirectory(${ELECTRON_THIRD_PARTY_DIR}/cppfront)
set(CPPFRONT_INCLUDE_DIR ${ELECTRON_THIRD_PARTY_SOURCE_DIR}/cppfront/include)

if(CPPFRONT_EXECUTABLE STREQUAL "")
  message(FATAL_ERROR "failed to resolve CPPFRONT_EXECUTABLE")
endif()

include(translate)

# cpp2
translate_include_directories(${CPPFRONT_INCLUDE_DIR} electron PUBLIC ${ELECTRON_SOURCE_DIR}/include)
target_translate_sources(electron PUBLIC ${ELECTRON_SOURCE_DIR}/src)

if(BUILD_ELECTRON_EXAMPLE)
  file(GLOB_RECURSE ELECTRON_EXAMPLE_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/example/src/*.cpp)
  add_executable(example ${ELECTRON_EXAMPLE_SOURCE_FILES})
  target_include_directories(example PRIVATE ${ELECTRON_SOURCE_DIR}/example/include)
  target_link_libraries(example PRIVATE electron)
  # cpp2
  target_translate_include_directories(example PRIVATE ${ELECTRON_SOURCE_DIR}/example/include)
  target_translate_sources(example PRIVATE ${ELECTRON_SOURCE_DIR}/example/src)
endif()

if(BUILD_ELECTRON_TESTING)
  include(CTest)
  enable_testing()
  include(exec)
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/test)
  set(test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test)
  exec_dir(${test_dir} "test-" ${test_output_dir} true electron)
endif()
