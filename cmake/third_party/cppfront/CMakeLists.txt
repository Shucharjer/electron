cmake_minimum_required(VERSION 3.21)

project(cppfront LANGUAGES CXX)

get_filename_component(ELECTRON_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)
set(CPPFRONT_SOURCE_DIR "${ELECTRON_ROOT_DIR}/third_party/cppfront")

add_executable(cppfront "${CPPFRONT_SOURCE_DIR}/source/cppfront.cpp")

target_include_directories(cppfront PRIVATE "${CPPFRONT_SOURCE_DIR}/source")
target_compile_features(cppfront PRIVATE cxx_std_20)

set_target_properties(cppfront PROPERTIES
  OUTPUT_NAME cppfront
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

set(CPPFRONT_EXECUTABLE "$<TARGET_FILE:cppfront>")
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CPPFRONT_EXECUTABLE "${CPPFRONT_EXECUTABLE}" PARENT_SCOPE)
endif()
