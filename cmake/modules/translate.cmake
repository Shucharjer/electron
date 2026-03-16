function(translate CPP2_SRC_DIR CPP_OUTPUT_DIR)
  file(GLOB_RECURSE CPP2_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false
    RELATIVE "${CPP2_SRC_DIR}"
    "${CPP2_SRC_DIR}/*.h2"
    "${CPP2_SRC_DIR}/*.cpp2"
  )

  set(GENERATED_SOURCES "")
  foreach(CPP2_FILE IN LISTS CPP2_FILES)
    if(CPP2_FILE MATCHES ".*\\.cpp2$")
      string(REGEX REPLACE "\\.cpp2$" ".cpp" CPP_FILE "${CPP2_FILE}")
    elseif(CPP2_FILE MATCHES ".*\\.h2$")
      string(REGEX REPLACE "\\.h2$" ".hpp" CPP_FILE "${CPP2_FILE}")
    else()
      continue()
    endif()

    set(input_file "${CPP2_SRC_DIR}/${CPP2_FILE}")
    set(output_file "${CPP_OUTPUT_DIR}/${CPP_FILE}")

    get_filename_component(output_dir "${output_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")

    add_custom_command(
      OUTPUT "${output_file}"
      COMMAND "${CPPFRONT_EXECUTABLE}" "${input_file}" -o "${output_file}" -cl -fno-rtti
      DEPENDS "${input_file}" "${CPPFRONT_EXECUTABLE}"
      COMMENT "Cpp2 -> Cpp: ${CPP_FILE}"
      VERBATIM
    )
    list(APPEND GENERATED_SOURCES "${output_file}")
  endforeach()

  if(GENERATED_SOURCES)
    set_source_files_properties(${GENERATED_SOURCES} PROPERTIES GENERATED TRUE)
  endif()

  set(TRANSLATED_SOURCES "${GENERATED_SOURCES}" PARENT_SCOPE)
endfunction()

function(register_translated_sources target visibility relative_subdir)
  set(translated_sources ${ARGN})
  if(NOT translated_sources)
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${target}_${relative_subdir}" translation_target_suffix)
  set(translation_target "${target}_translate_${translation_target_suffix}")

  if(NOT TARGET "${translation_target}")
    add_custom_target("${translation_target}" DEPENDS ${translated_sources})
  endif()

  add_dependencies("${target}" "${translation_target}")
  target_sources("${target}" ${visibility} ${translated_sources})
endfunction()

function(target_translate_sources target visibility)
  set(dirs ${ARGN})
  foreach(dir ${dirs})
    file(RELATIVE_PATH RELATIVE_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}")
    set(CPP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${RELATIVE_SUBDIR}")
    translate("${dir}" "${CPP_OUTPUT_DIR}")
    register_translated_sources("${target}" "${visibility}" "${RELATIVE_SUBDIR}" ${TRANSLATED_SOURCES})
  endforeach()
endfunction()

function(target_translate_include_directories target visibility)
  set(dirs ${ARGN})
  foreach(dir ${dirs})
    file(RELATIVE_PATH RELATIVE_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}")
    set(CPP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${RELATIVE_SUBDIR}")
    translate("${dir}" "${CPP_OUTPUT_DIR}")
    register_translated_sources("${target}" "${visibility}" "${RELATIVE_SUBDIR}" ${TRANSLATED_SOURCES})
    target_include_directories("${target}" ${visibility} "${CPP_OUTPUT_DIR}")
  endforeach()
endfunction()

function(translate_include_directories CPPFRONT_INCLUDE_DIR target visibility)
  set(dirs ${ARGN})
  foreach(dir ${dirs})
    file(RELATIVE_PATH RELATIVE_SUBDIR "${CMAKE_CURRENT_SOURCE_DIR}" "${dir}")
    set(CPP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${RELATIVE_SUBDIR}")
    translate("${dir}" "${CPP_OUTPUT_DIR}")
    register_translated_sources("${target}" "${visibility}" "${RELATIVE_SUBDIR}" ${TRANSLATED_SOURCES})
    target_include_directories("${target}" ${visibility} "${CPP_OUTPUT_DIR}")
  endforeach()

  target_include_directories("${target}" ${visibility} "${CPPFRONT_INCLUDE_DIR}")
endfunction()
