# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
set(fixture_options)
foreach(option Qt6_DIR CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    if(DEFINED ${option} AND NOT "${${option}}" STREQUAL "")
        list(APPEND fixture_options "-D${option}=${${option}}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}/fixture" -B "${TEST_BINARY_DIR}/build"
    "-DDESKTOP_SOURCE_DIR=${DESKTOP_SOURCE_DIR}"
    -DCMAKE_INSTALL_PREFIX=/usr
    "-DPKG_CONFIG_EXECUTABLE=${PKG_CONFIG_EXECUTABLE}"
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
    "-DMETADATA=${METADATA}"
    ${fixture_options}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not configure CloudProviders install fixture")
endif()

set(stage "${TEST_BINARY_DIR}/stage")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${stage}"
    "${CMAKE_COMMAND}" --install "${TEST_BINARY_DIR}/build"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not install CloudProviders fixture")
endif()

file(STRINGS "${TEST_BINARY_DIR}/build/identity" identity)
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/user-data" "${TEST_BINARY_DIR}/config")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "XDG_DATA_DIRS=${stage}/usr/share"
    "XDG_DATA_HOME=${TEST_BINARY_DIR}/user-data"
    "XDG_CONFIG_HOME=${TEST_BINARY_DIR}/config"
    "${DBUS_RUN_SESSION_EXECUTABLE}" -- "${DISCOVERY_TEST}" ${identity}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "CloudProviders passive discovery failed: ${result}")
endif()
