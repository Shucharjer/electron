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
  -o ${EXTERNAL_DIR}/${CPPFRONT_NAME})
ExternalProject_Add(
  cppfront
  PREFIX ${EXTERNAL_DIR}
  GIT_REPOSITORY https://github.com/hsutter/cppfront.git
  GIT_TAG main
  GIT_SHALLOW 1
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${CPPFRONT_COMPILE_COMMAND}
  BUILD_ALWAYS OFF
  INSTALL_COMMAND ""
  TEST_COMMAND ""
)
set(CPPFRONT_EXECUTABLE ${EXTERNAL_DIR}/${CPPFRONT_NAME})

include(translate)
translate(${CPPFRONT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
target_include_directories(electron PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
translate(${CPPFRONT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/generated/src)
target_sources(electron PUBLIC ${TRANSLATED_SOURCES})

if(ELECTRON_BUILD_EXAMPLE)
  file(GLOB_RECURSE ELECTRON_EXAMPLE_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/example/src/*.cpp)
  translate(${CPPFRONT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example/include ${CMAKE_CURRENT_BINARY_DIR}/generated/example/include)
  message("translated: ${TRANSLATED_SOURCES}")
  translate(${CPPFRONT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example/src ${CMAKE_CURRENT_BINARY_DIR}/generated/example/src)
  message("translated: ${TRANSLATED_SOURCES}")
  add_executable(example ${ELECTRON_EXAMPLE_SOURCE_FILES} ${TRANSLATED_SOURCES})
  target_include_directories(example PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/example/include)
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