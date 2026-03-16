cmake_minimum_required(VERSION 3.21)

project(imgui LANGUAGES CXX)

get_filename_component(ELECTRON_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)
set(IMGUI_SOURCE_DIR "${ELECTRON_ROOT_DIR}/third_party/imgui")
set(GLFW_SOURCE_DIR "${ELECTRON_ROOT_DIR}/third_party/glfw")
set(VULKAN_HEADERS_SOURCE_DIR "${ELECTRON_ROOT_DIR}/third_party/vulkan-headers")

set(IMGUI_STAGE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(IMGUI_STAGE_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")

set(IMGUI_HEADER_FILES
  imconfig.h
  imgui.h
  imgui_internal.h
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
  backends/imgui_impl_glfw.h
  backends/imgui_impl_vulkan.h
)

set(IMGUI_SOURCE_FILES
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  backends/imgui_impl_glfw.cpp
  backends/imgui_impl_vulkan.cpp
)

function(stage_imgui_files output_variable stage_root)
  set(staged_files)

  foreach(relative_path IN LISTS ARGN)
    set(source_path "${IMGUI_SOURCE_DIR}/${relative_path}")
    set(staged_path "${CMAKE_CURRENT_BINARY_DIR}/${stage_root}/${relative_path}")

    get_filename_component(staged_dir "${staged_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${staged_dir}")
    configure_file("${source_path}" "${staged_path}" COPYONLY)

    list(APPEND staged_files "${staged_path}")
  endforeach()

  set(${output_variable} "${staged_files}" PARENT_SCOPE)
endfunction()

stage_imgui_files(IMGUI_STAGED_HEADERS include ${IMGUI_HEADER_FILES})
stage_imgui_files(IMGUI_STAGED_SOURCES src ${IMGUI_SOURCE_FILES})

add_library(imgui STATIC)
add_library(imgui::imgui ALIAS imgui)

target_sources(imgui
  PRIVATE
    ${IMGUI_STAGED_HEADERS}
    ${IMGUI_STAGED_SOURCES}
)

target_include_directories(imgui
  PUBLIC
    "${IMGUI_STAGE_INCLUDE_DIR}"
    "${IMGUI_STAGE_INCLUDE_DIR}/backends"
    "${GLFW_SOURCE_DIR}/include"
    "${VULKAN_HEADERS_SOURCE_DIR}/include"
)

target_compile_features(imgui PUBLIC cxx_std_20)
