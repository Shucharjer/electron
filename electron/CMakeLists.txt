project(
    electron
    LANGUAGES CXX
)

# CMake

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules)

option(ELECTRON_BUILD_EXAMPLE OFF)
option(ELECTRON_BUILD_TEST OFF)

set(ELECTRON_SOURCE_DIR ${electron_SOURCE_DIR})

file(GLOB_RECURSE ELECTRON_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/src/*.cpp)
add_library(electron STATIC ${ELECTRON_SOURCE_FILES})
target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/include)
target_link_libraries(electron PUBLIC proton)

find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
target_link_libraries(electron PUBLIC imgui::imgui SDL3::SDL3 Vulkan::Vulkan)

target_include_directories(electron PUBLIC ${ELECTRON_SOURCE_DIR}/third_party/include)
file(GLOB_RECURSE ELECTRON_THIRD_PARTY_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/third_party/src/*.cpp)
target_sources(electron PRIVATE ${ELECTRON_THIRD_PARTY_SOURCE_FILES})

if(ELECTRON_BUILD_EXAMPLE)
  file(GLOB_RECURSE ELECTRON_EXAMPLE_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/example/src/*.cpp)
  add_executable(example ${ELECTRON_EXAMPLE_SOURCE_FILES})
  target_link_libraries(example PRIVATE electron)
endif()

if(ELECTRON_BUILD_TEST)
  include(CTest)
  enable_testing()
  include(exec)
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/test)
  set(test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test)
  exec_dir(${test_dir} "test-" ${test_output_dir} true electron)
endif()