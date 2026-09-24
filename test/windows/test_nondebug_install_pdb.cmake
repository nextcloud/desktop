# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

foreach(required_variable IN ITEMS PDB_INSTALL_TEST_BUILD_DIR PDB_INSTALL_TEST_CONFIG)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be set.")
    endif()
endforeach()

if(PDB_INSTALL_TEST_CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "PDB install check requires a non-Debug build.")
endif()

get_filename_component(build_dir "${PDB_INSTALL_TEST_BUILD_DIR}" ABSOLUTE)
set(install_root "${build_dir}/test/pdb-install-test")

file(REMOVE_RECURSE "${install_root}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${build_dir}"
        --config "${PDB_INSTALL_TEST_CONFIG}"
        --prefix "${install_root}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)

if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "${PDB_INSTALL_TEST_CONFIG} installation failed:\n${install_output}\n${install_error}")
endif()

file(GLOB_RECURSE installed_files LIST_DIRECTORIES false "${install_root}/*")
if(NOT installed_files)
    message(FATAL_ERROR "${PDB_INSTALL_TEST_CONFIG} installation produced no files.")
endif()
foreach(installed_file IN LISTS installed_files)
    if(installed_file MATCHES "\\.[Pp][Dd][Bb]$")
        message(FATAL_ERROR "${PDB_INSTALL_TEST_CONFIG} installed a PDB: ${installed_file}")
    endif()
endforeach()

file(REMOVE_RECURSE "${install_root}")
message(STATUS "${PDB_INSTALL_TEST_CONFIG} installation contains no PDBs.")
