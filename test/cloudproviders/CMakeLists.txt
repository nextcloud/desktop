# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CloudProvidersDiscoveryTest LANGUAGES C)
    enable_testing()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CLOUDPROVIDERS REQUIRED cloudproviders IMPORTED_TARGET)
    pkg_check_modules(GIO REQUIRED gio-2.0 IMPORTED_TARGET)
endif()

find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(NOT DBUS_RUN_SESSION_EXECUTABLE)
    message(STATUS "Skipping CloudProviders discovery tests: dbus-run-session is unavailable.")
    return()
endif()

add_executable(CloudProvidersDiscoveryTest testdiscovery.c)
target_link_libraries(CloudProvidersDiscoveryTest PRIVATE PkgConfig::CLOUDPROVIDERS PkgConfig::GIO)
set_target_properties(CloudProvidersDiscoveryTest PROPERTIES AUTOMOC OFF)

set(fixture_options)
foreach(option Qt6_DIR CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    if(DEFINED ${option} AND NOT "${${option}}" STREQUAL "")
        list(APPEND fixture_options "-D${option}=${${option}}")
    endif()
endforeach()

foreach(metadata desktop legacy)
    add_test(NAME CloudProvidersDiscovery_${metadata}
        COMMAND "${CMAKE_COMMAND}"
            "-DDESKTOP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../.."
            "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/${metadata}"
            "-DDISCOVERY_TEST=$<TARGET_FILE:CloudProvidersDiscoveryTest>"
            "-DDBUS_RUN_SESSION_EXECUTABLE=${DBUS_RUN_SESSION_EXECUTABLE}"
            "-DPKG_CONFIG_EXECUTABLE=${PKG_CONFIG_EXECUTABLE}"
            "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            "-DMETADATA=${metadata}"
            ${fixture_options}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run.cmake")
    set_tests_properties(CloudProvidersDiscovery_${metadata} PROPERTIES TIMEOUT 30)
endforeach()
