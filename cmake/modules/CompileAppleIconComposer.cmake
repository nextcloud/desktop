# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Locate `actool` once. Prefer the version inside the active Xcode toolchain
# (xcrun), falling back to whatever is on PATH.
if(NOT ACTOOL_EXECUTABLE)
    execute_process(
        COMMAND xcrun --find actool
        OUTPUT_VARIABLE _actool_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _actool_lookup
        ERROR_QUIET)
    if(_actool_lookup EQUAL 0 AND EXISTS "${_actool_path}")
        set(ACTOOL_EXECUTABLE "${_actool_path}" CACHE FILEPATH "Apple asset catalog compiler")
    else()
        find_program(ACTOOL_EXECUTABLE actool)
    endif()
    if(NOT ACTOOL_EXECUTABLE)
        message(FATAL_ERROR "actool not found. Install Xcode (or the Command Line Tools) to compile Icon Composer .icon sources.")
    endif()
endif()

# Compile an Icon Composer `.icon` bundle into the modern macOS asset catalog
# format (`Assets.car`) plus a loose `<app-icon-name>.icns` fallback.
#
#   compile_apple_icon_composer(<icon_source> <out_dir> <out_var>)
#
# Arguments:
#   icon_source  Absolute path to the source `.icon` directory.
#   out_dir      Directory where actool writes its output (Assets.car,
#                <app-icon-name>.icns, partial Info.plist). Created on demand.
#   out_var      Name of a parent-scope variable that receives the list of
#                generated source files to attach to the macOS bundle target.
#                Outputs are tagged with MACOSX_PACKAGE_LOCATION=Resources so
#                they land in `<app>.app/Contents/Resources/` automatically.
#
# actool requires its input argument to be a *parent directory* containing the
# `.icon`, not the `.icon` itself, so we stage the icon into a build-side
# folder first.
function(compile_apple_icon_composer icon_source out_dir out_var)
    get_filename_component(_icon_name "${icon_source}" NAME_WE)
    set(_staging_dir "${out_dir}/Sources")
    set(_staged_icon "${_staging_dir}/${_icon_name}.icon")
    set(_assets_car "${out_dir}/Assets.car")
    set(_app_icns "${out_dir}/${_icon_name}.icns")
    set(_partial_plist "${out_dir}/${_icon_name}.partial.plist")

    file(MAKE_DIRECTORY "${out_dir}" "${_staging_dir}")

    add_custom_command(
        OUTPUT "${_assets_car}" "${_app_icns}" "${_partial_plist}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_staged_icon}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${icon_source}" "${_staged_icon}"
        COMMAND "${ACTOOL_EXECUTABLE}"
                "${_staged_icon}"
                --platform macosx
                --minimum-deployment-target "${CMAKE_OSX_DEPLOYMENT_TARGET}"
                --target-device mac
                --app-icon "${_icon_name}"
                --standalone-icon-behavior all
                --compile "${out_dir}"
                --output-partial-info-plist "${_partial_plist}"
                --errors --warnings
                --output-format human-readable-text
        DEPENDS "${icon_source}/icon.json"
        WORKING_DIRECTORY "${out_dir}"
        COMMENT "Compiling Icon Composer bundle ${_icon_name}.icon with actool"
        VERBATIM)

    set_source_files_properties("${_assets_car}" "${_app_icns}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
        GENERATED TRUE)

    set(${out_var} "${_assets_car}" "${_app_icns}" PARENT_SCOPE)
endfunction()
