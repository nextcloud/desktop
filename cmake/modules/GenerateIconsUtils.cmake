# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: BSD-3-Clause
# find_package(ECM 1.7.0 REQUIRED NO_MODULE)
# list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMAddAppIcon)

# Sets <result> to the path of svgtopng, the tool that renders the icons with Qt Svg as the client does at runtime.
# The tool is built in the build directory when it is missing or older than its source.
function(build_svg_to_png_tool result)
  set(source_dir "${CMAKE_SOURCE_DIR}/cmake/svgtopng")
  set(binary_dir "${CMAKE_BINARY_DIR}/svgtopng")
  set(tool "${binary_dir}/svgtopng${CMAKE_EXECUTABLE_SUFFIX}")

  if ("${source_dir}/main.cpp" IS_NEWER_THAN "${tool}")
    message(STATUS "Build svgtopng")
    # On Windows the tool must use the same C runtime as Qt. A release build does so with a release build of Qt,
    # whatever the client is built as.
    set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    # try_compile hands the generator and the compiler of the client to the project, but none of these settings.
    try_compile(SVG_TO_PNG_TOOL_BUILT "${binary_dir}" "${source_dir}" svgtopng
      CMAKE_FLAGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DQt6_DIR=${Qt6_DIR}"
        "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
      OUTPUT_VARIABLE build_output)

    if (NOT SVG_TO_PNG_TOOL_BUILT)
      message(FATAL_ERROR "Could not build svgtopng:\n${build_output}")
    endif()
  endif()

  set(${result} "${tool}" PARENT_SCOPE)
endfunction()

function(generate_sized_png_from_svg icon_path size)
  set(options)
  set(oneValueArgs OUTPUT_ICON_NAME OUTPUT_ICON_FULL_NAME_WLE OUTPUT_ICON_PATH)
  set(multiValueArgs)

  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  get_filename_component(icon_name_dir ${icon_path} DIRECTORY)
  get_filename_component(icon_name_wle ${icon_path} NAME_WLE)

  if (ARG_OUTPUT_ICON_NAME)
    set(icon_name_wle ${ARG_OUTPUT_ICON_NAME})
  endif ()

  if (ARG_OUTPUT_ICON_PATH)
    set(icon_name_dir ${ARG_OUTPUT_ICON_PATH})
  endif ()

  set(output_icon_full_name_wle "${size}-${icon_name_wle}")

  if (ARG_OUTPUT_ICON_FULL_NAME_WLE)
    set(output_icon_full_name_wle ${ARG_OUTPUT_ICON_FULL_NAME_WLE})
  endif ()

  if (EXISTS "${icon_name_dir}/${output_icon_full_name_wle}.png")
    return()
  endif()

  set(icon_output_name "${output_icon_full_name_wle}.png")
  message(STATUS "Generate ${icon_output_name}")
  build_svg_to_png_tool(svg_to_png_tool)

  if (CMAKE_HOST_WIN32)
    # Windows has no run path: the tool finds the Qt libraries through PATH only.
    get_target_property(qt_core_library Qt6::Core LOCATION)
    get_filename_component(qt_library_dir "${qt_core_library}" DIRECTORY)
    set(path_before "$ENV{PATH}")
    set(ENV{PATH} "${qt_library_dir};$ENV{PATH}")
  endif()

  execute_process(COMMAND
    "${svg_to_png_tool}" ${size} "${icon_path}" "${icon_output_name}"
    WORKING_DIRECTORY "${icon_name_dir}"
    RESULT_VARIABLE
    SVG_CONVERTER_SIDEBAR_ERROR
    OUTPUT_QUIET
    ERROR_VARIABLE svg_to_png_error)

  if (CMAKE_HOST_WIN32)
    set(ENV{PATH} "${path_before}")
  endif()

  if (SVG_CONVERTER_SIDEBAR_ERROR)
    message(FATAL_ERROR
      "svgtopng could not generate ${icon_output_name} from ${icon_path}: ${SVG_CONVERTER_SIDEBAR_ERROR}\n${svg_to_png_error}")
  endif()
endfunction()
