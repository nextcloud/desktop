# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: BSD-3-Clause
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

# Separator used to pack a queued conversion job into a single list element.
set(GENERATE_ICONS_JOB_SEPARATOR "@@@")

# Queue a single SVG -> PNG conversion. Nothing is generated until
# run_generate_queued_pngs_from_svg() is called to run the queued jobs.
function(queue_generate_sized_png_from_svg icon_path size)
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

  set(icon_output_path "${icon_name_dir}/${output_icon_full_name_wle}.png")

  if (EXISTS "${icon_output_path}")
    return()
  endif()

  # Pack size, source and destination into one element so the flat CMake list
  # keeps one entry per job. run_generate_queued_pngs_from_svg() unpacks it again.
  set(job "${size}${GENERATE_ICONS_JOB_SEPARATOR}${icon_path}${GENERATE_ICONS_JOB_SEPARATOR}${icon_output_path}")
  set_property(GLOBAL APPEND PROPERTY GENERATE_ICONS_PENDING_JOBS "${job}")
endfunction()

# Run every conversion queued by queue_generate_sized_png_from_svg() so far.
#
# Two strategies are used depending on the converter:
#  * rsvg-convert (and any other converter) is launched as independent processes
#    in batches of at most one process per logical core. Within a batch every
#    converter runs concurrently, because execute_process() starts all of its
#    COMMAND entries at once.
#  * Inkscape must not be run as many concurrent instances: separate instances
#    race during GTK/fontconfig start-up and abort. It is instead driven through
#    its single-process "--shell" batch mode, which does every conversion in one
#    process and thereby avoids paying the expensive Inkscape start-up cost once
#    per icon.

function(run_generate_queued_pngs_from_svg)
  get_property(pending_jobs GLOBAL PROPERTY GENERATE_ICONS_PENDING_JOBS)

  if (NOT pending_jobs)
    return()
  endif()

  # Clear the queue up front so a later call does not reprocess these jobs.
  set_property(GLOBAL PROPERTY GENERATE_ICONS_PENDING_JOBS "")

  list(LENGTH pending_jobs job_count)

  get_filename_component(converter_name "${SVG_CONVERTER}" NAME_WE)
  string(TOLOWER "${converter_name}" converter_name)

  if (converter_name MATCHES "inkscape")
    message(STATUS "Generating ${job_count} icon(s) from SVG in one Inkscape batch")
    _run_inkscape_shell_jobs("${pending_jobs}")
    return()
  endif()

  cmake_host_system_information(RESULT batch_size QUERY NUMBER_OF_LOGICAL_CORES)
  if (NOT batch_size OR batch_size LESS 1)
    set(batch_size 4)
  endif()

  message(STATUS "Generating ${job_count} icon(s) from SVG, up to ${batch_size} in parallel")

  set(batch_index 0)
  set(process_args "")

  foreach(job IN LISTS pending_jobs)
    string(REPLACE "${GENERATE_ICONS_JOB_SEPARATOR}" ";" job_parts "${job}")
    list(GET job_parts 0 job_size)
    list(GET job_parts 1 job_input)
    list(GET job_parts 2 job_output)

    list(APPEND process_args
      COMMAND "${SVG_CONVERTER}" -w ${job_size} -h ${job_size} "${job_input}" -o "${job_output}")

    math(EXPR batch_index "${batch_index} + 1")

    if (batch_index GREATER_EQUAL batch_size)
      _run_png_conversion_batch("${process_args}")
      set(batch_index 0)
      set(process_args "")
    endif()
  endforeach()

  if (process_args)
    _run_png_conversion_batch("${process_args}")
  endif()
endfunction()

# Execute one batch of converter invocations concurrently and fail hard if any
# of them reports an error. Not intended to be called directly.
function(_run_png_conversion_batch process_args)
  execute_process(${process_args}
    RESULTS_VARIABLE batch_results
    OUTPUT_QUIET
    ERROR_QUIET)

  foreach(result IN LISTS batch_results)
    if (result)
      message(FATAL_ERROR
        "${SVG_CONVERTER} could not generate icon: ${result}")
    endif()
  endforeach()
endfunction()

# Convert every queued job in a single Inkscape process using its shell mode.
# Not intended to be called directly.
function(_run_inkscape_shell_jobs jobs)
  set(shell_script "")
  set(expected_outputs "")

  foreach(job IN LISTS jobs)
    string(REPLACE "${GENERATE_ICONS_JOB_SEPARATOR}" ";" job_parts "${job}")
    list(GET job_parts 0 job_size)
    list(GET job_parts 1 job_input)
    list(GET job_parts 2 job_output)

    string(APPEND shell_script
      "file-open:${job_input}; export-width:${job_size}; export-height:${job_size}; export-filename:${job_output}; export-do; file-close\n")
    list(APPEND expected_outputs "${job_output}")
  endforeach()

  string(APPEND shell_script "quit\n")

  if (DEFINED CMAKE_CURRENT_BINARY_DIR AND CMAKE_CURRENT_BINARY_DIR)
    set(shell_script_dir "${CMAKE_CURRENT_BINARY_DIR}")
  else()
    set(shell_script_dir "${CMAKE_BINARY_DIR}")
  endif()
  set(shell_script_file "${shell_script_dir}/inkscape-icon-batch.txt")
  file(WRITE "${shell_script_file}" "${shell_script}")

  execute_process(COMMAND "${SVG_CONVERTER}" --shell
    INPUT_FILE "${shell_script_file}"
    RESULT_VARIABLE inkscape_result
    OUTPUT_QUIET
    ERROR_QUIET)

  file(REMOVE "${shell_script_file}")

  # Inkscape's shell mode may keep a zero exit code even when an individual
  # export fails, so verify every expected file was actually produced.
  foreach(expected_output IN LISTS expected_outputs)
    if (NOT EXISTS "${expected_output}")
      message(FATAL_ERROR
        "${SVG_CONVERTER} could not generate icon: ${expected_output}")
    endif()
  endforeach()
endfunction()
