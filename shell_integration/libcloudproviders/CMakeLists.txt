# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
macro(libcloudproviders_add_config _sources)
    set(_install_dir "${CMAKE_INSTALL_PREFIX}/share/cloud-providers")
    foreach (_i ${_sources})
        get_filename_component(_service_file ${_i} ABSOLUTE)
        string(REGEX REPLACE "\\.ini.*$" ".ini" _output_file ${_i})
        set(_target ${CMAKE_CURRENT_BINARY_DIR}/${_output_file})
        configure_file(${_service_file} ${_target})
        install(FILES ${_target} DESTINATION ${_install_dir} RENAME "${LIBCLOUDPROVIDERS_DBUS_BUS_NAME}.ini")
    endforeach (_i ${ARGN})
endmacro()


if (Qt6_FOUND)
    find_package(Qt6 COMPONENTS DBus)
else()
    set(REQUIRED_QT_VERSION "5.15.0")
    find_package(Qt5 ${REQUIRED_QT_VERSION} COMPONENTS DBus)
endif()
IF (Qt5DBus_FOUND OR Qt6DBus_FOUND)
    STRING(TOLOWER "${APPLICATION_VENDOR}" DBUS_VENDOR)
    STRING(REGEX REPLACE "[^A-z0-9]" "" DBUS_VENDOR "${DBUS_VENDOR}")
    STRING(REGEX REPLACE "[^A-z0-9]" "" DBUS_APPLICATION_NAME "${APPLICATION_SHORTNAME}")
    if (NOT DBUS_PREFIX)
        set(DBUS_PREFIX "com")
    endif ()

    set(LIBCLOUDPROVIDERS_DBUS_BUS_NAME "${DBUS_PREFIX}.${DBUS_VENDOR}.${DBUS_APPLICATION_NAME}")
    set(LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH "/${DBUS_PREFIX}/${DBUS_VENDOR}/${DBUS_APPLICATION_NAME}")

    # The .ini file has been replaced by a declaration in the .desktop file in 0.3.3+
    if (${CLOUDPROVIDERS_VERSION} VERSION_LESS "0.3.3")
        libcloudproviders_add_config(org.freedesktop.CloudProviders.ini.in)
    endif ()
ENDIF ()
