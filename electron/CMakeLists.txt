project(
    electron
    LANGUAGES CXX
)

# CMake

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules)

option(ELECTRON_BUILD_EXAMPLE OFF)
option(ELECTRON_BUILD_TEST OFF)
option(ELECTRON_USES_LOCAL_CPPFRONT OFF)

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

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

include(translate)

# cpp2
translate_include_directories(${CPPFRONT_EXECUTABLE} ${CPPFRONT_INCLUDE_DIR} electron PUBLIC ${ELECTRON_SOURCE_DIR}/include)
target_translate_sources(${CPPFRONT_EXECUTABLE} electron PUBLIC ${ELECTRON_SOURCE_DIR}/src)

if(ELECTRON_BUILD_EXAMPLE)
  file(GLOB_RECURSE ELECTRON_EXAMPLE_SOURCE_FILES ${ELECTRON_SOURCE_DIR}/example/src/*.cpp)
  add_executable(example ${ELECTRON_EXAMPLE_SOURCE_FILES})
  target_link_libraries(example PRIVATE electron)
  # cpp2
  target_translate_include_directories(${CPPFRONT_EXECUTABLE} example PRIVATE ${ELECTRON_SOURCE_DIR}/example/include)
  target_translate_sources(${CPPFRONT_EXECUTABLE} example PRIVATE ${ELECTRON_SOURCE_DIR}/example/src)
endif()

if(ELECTRON_BUILD_TEST)
  include(CTest)
  enable_testing()
  include(exec)
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/test)
  set(test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test)
  exec_dir(${test_dir} "test-" ${test_output_dir} true electron)
endif()