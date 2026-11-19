# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CloudProvidersDiscoveryTest LANGUAGES C)
    enable_testing()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CLOUDPROVIDERS REQUIRED cloudproviders>=0.3.3 IMPORTED_TARGET)
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

add_test(NAME CloudProvidersDiscovery
    COMMAND "${CMAKE_COMMAND}"
        "-DDESKTOP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../.."
        "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/discovery"
        "-DDISCOVERY_TEST=$<TARGET_FILE:CloudProvidersDiscoveryTest>"
        "-DDBUS_RUN_SESSION_EXECUTABLE=${DBUS_RUN_SESSION_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run.cmake")
set_tests_properties(CloudProvidersDiscovery PROPERTIES TIMEOUT 30)
