# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}/fixture" -B "${TEST_BINARY_DIR}/build"
    "-DDESKTOP_SOURCE_DIR=${DESKTOP_SOURCE_DIR}"
    -DCMAKE_INSTALL_PREFIX=/usr
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not configure CloudProviders install fixture")
endif()

file(STRINGS "${TEST_BINARY_DIR}/build/identity" identity)
list(GET identity 0 bus_name)
set(stage "${TEST_BINARY_DIR}/stage")
set(service_file "${stage}/usr/share/dbus-1/services/${bus_name}.service")
file(WRITE "${service_file}" "[D-BUS Service]\nName=${bus_name}\nExec=/usr/bin/nextcloud --background\n\n")
set(custom_service_file "${stage}/legacy/share/dbus-1/services/${bus_name}.service")
set(custom_service "[D-BUS Service]\nName=${bus_name}\nExec=/opt/nextcloud/bin/nextcloud --background\n\n")
file(WRITE "${custom_service_file}" "${custom_service}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${stage}"
    "${CMAKE_COMMAND}" --install "${TEST_BINARY_DIR}/build"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not install CloudProviders fixture")
endif()

if(EXISTS "${service_file}")
    message(FATAL_ERROR "Upgrade left the obsolete CloudProviders service installed")
endif()

file(READ "${custom_service_file}" installed_custom_service)
if(NOT installed_custom_service STREQUAL custom_service)
    message(FATAL_ERROR "Upgrade changed an activation service belonging to a different prefix")
endif()

file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/user-data" "${TEST_BINARY_DIR}/config" "${TEST_BINARY_DIR}/runtime")
# The default session config also searches the host's compiled-in service directory.
set(bus_config "${TEST_BINARY_DIR}/session.conf")
configure_file("${CMAKE_CURRENT_LIST_DIR}/session.conf" "${bus_config}" COPYONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "XDG_DATA_DIRS=${stage}/usr/share"
    "XDG_DATA_HOME=${TEST_BINARY_DIR}/user-data"
    "XDG_RUNTIME_DIR=${TEST_BINARY_DIR}/runtime"
    "XDG_CONFIG_HOME=${TEST_BINARY_DIR}/config"
    "${DBUS_RUN_SESSION_EXECUTABLE}" "--config-file=${bus_config}" -- "${DISCOVERY_TEST}" ${identity}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "CloudProviders passive discovery failed: ${result}")
endif()
