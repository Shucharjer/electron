project(
    electron
    LANGUAGES CXX
)

# CMake

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules)

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
target_link_libraries(electron PUBLIC proton)

find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
target_link_libraries(electron PUBLIC
  imgui::imgui
  Vulkan::Vulkan # for sdl & imgui impl
  Vulkan::Headers
  GPUOpen::VulkanMemoryAllocator
  unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)

if (ELECTRON_USES_SDL3)
  target_compile_definitions(electron PUBLIC ELECTRON_USES_SDL3_VULKAN)
  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(electron PUBLIC SDL3::SDL3)
  target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/third_party/imgui/sdl3/include)
  file(GLOB_RECURSE ELECTRON_THIRD_PARTY_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/third_party/imgui/sdl3/src/*.cpp)
  target_sources(electron PRIVATE ${ELECTRON_THIRD_PARTY_SOURCE_FILES})
else()
  target_compile_definitions(electron PUBLIC ELECTRON_USES_SDL2_VULKAN)
  find_package(SDL2 CONFIG REQUIRED)
  target_link_libraries(electron PUBLIC SDL2::SDL2)
  target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/third_party/imgui/sdl2/include)
  file(GLOB_RECURSE ELECTRON_THIRD_PARTY_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/third_party/imgui/sdl2/src/*.cpp)
  target_sources(electron PRIVATE ${ELECTRON_THIRD_PARTY_SOURCE_FILES})
endif()

target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/third_party/imgui/api/vulkan/include)
file(GLOB_RECURSE ELECTRON_THIRD_PARTY_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/third_party/imgui/api/vulkan/src/*.cpp)
target_sources(electron PRIVATE ${ELECTRON_THIRD_PARTY_SOURCE_FILES})

if(ELECTRON_USES_LOCAL_CPPFRONT)
  set(CPPFRONT_EXECUTABLE "" CACHE FILEPATH "cppfront executable")
  set(CPPFRONT_INCLUDE_DIR "" CACHE DIRECTORY "cppfront include directory")
  if(CPPFRONT_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "please set path for local cppfront executable")
  endif()
  if(CPPFRONT_INCLUDE_DIR STREQUAL "")
    message(FATAL_ERROR "please set path for local cppfront include directory")
  endif()
else()
  include(ExternalProject)
  if (WIN32)
    set(CPPFRONT_NAME "cppfront.exe")
  else()
    set(CPPFRONT_NAME "cppfront")
  endif()
  set(EXTERNAL_DIR ${CMAKE_BINARY_DIR}/external)
  set(CPPFRONT_SOURCE_DIR ${EXTERNAL_DIR}/src/cppfront)
  set(CPPFRONT_COMPILE_COMMAND ${CMAKE_CXX_COMPILER}
    -O3
    -DNDEBUG
    -D_NDEBUG
    -std=c++20
    -I ${CPPFRONT_SOURCE_DIR}/source
    ${CPPFRONT_SOURCE_DIR}/source/cppfront.cpp
    -o ${EXTERNAL_DIR}/bin/${CPPFRONT_NAME})
  file(MAKE_DIRECTORY ${EXTERNAL_DIR}/bin)
  ExternalProject_Add(
    cppfront
    PREFIX ${EXTERNAL_DIR}
    GIT_REPOSITORY https://github.com/hsutter/cppfront.git
    GIT_TAG main
    GIT_SHALLOW 1
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CPPFRONT_COMPILE_COMMAND}
    BUILD_ALWAYS OFF
    INSTALL_COMMAND ""
    TEST_COMMAND ""
  )
  set(CPPFRONT_EXECUTABLE ${EXTERNAL_DIR}/bin/${CPPFRONT_NAME})
  set(CPPFRONT_INCLUDE_DIR ${EXTERNAL_DIR}/src/cppfront/include)
endif()

include(translate)

# cpp2
translate_include_directories(${CPPFRONT_EXECUTABLE} ${CPPFRONT_INCLUDE_DIR} electron PUBLIC ${ELECTRON_SOURCE_DIR}/include)
target_translate_sources(${CPPFRONT_EXECUTABLE} electron PUBLIC ${ELECTRON_SOURCE_DIR}/src)

if(BUILD_ELECTRON_EXAMPLE)
  file(GLOB_RECURSE ELECTRON_EXAMPLE_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/example/src/*.cpp)
  add_executable(example ${ELECTRON_EXAMPLE_SOURCE_FILES})
  target_include_directories(example PRIVATE ${ELECTRON_SOURCE_DIR}/example/include)
  target_link_libraries(example PRIVATE electron)
  # cpp2
  target_translate_include_directories(${CPPFRONT_EXECUTABLE} example PRIVATE ${ELECTRON_SOURCE_DIR}/example/include)
  target_translate_sources(${CPPFRONT_EXECUTABLE} example PRIVATE ${ELECTRON_SOURCE_DIR}/example/src)
endif()

if(BUILD_ELECTRON_TESTING)
  include(CTest)
  enable_testing()
  include(exec)
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/test)
  set(test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test)
  exec_dir(${test_dir} "test-" ${test_output_dir} true electron)
endif()