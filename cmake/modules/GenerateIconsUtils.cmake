# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# find_package(ECM 1.7.0 REQUIRED NO_MODULE)
# list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMAddAppIcon)

find_program(SVG_CONVERTER
  NAMES inkscape inkscape.exe rsvg-convert
  REQUIRED
  HINTS "C:\\Program Files\\Inkscape\\bin" "/usr/bin" ENV SVG_CONVERTER_DIR)
# REQUIRED keyword is only supported on CMake 3.18 and above
if (NOT SVG_CONVERTER)
  message(FATAL_ERROR "Could not find a suitable svg converter. Set SVG_CONVERTER_DIR to the path of either the inkscape or rsvg-convert executable.")
endif()

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
  execute_process(COMMAND
    "${SVG_CONVERTER}" -w ${size} -h ${size} "${icon_path}" -o "${icon_output_name}"
    WORKING_DIRECTORY "${icon_name_dir}"
    RESULT_VARIABLE
    SVG_CONVERTER_SIDEBAR_ERROR
    OUTPUT_QUIET
    ERROR_QUIET)

  if (SVG_CONVERTER_SIDEBAR_ERROR)
    message(FATAL_ERROR
      "${SVG_CONVERTER} could not generate icon: ${SVG_CONVERTER_SIDEBAR_ERROR}")
  else()
  endif()
endfunction()
