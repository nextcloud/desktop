# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(CloudProvidersInstallFixture LANGUAGES C CXX)
include(GNUInstallDirs)
include("${DESKTOP_SOURCE_DIR}/NEXTCLOUD.cmake")
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS DBus)

if(METADATA STREQUAL "legacy")
    set(CLOUDPROVIDERS_VERSION 0.3.2)
else()
    pkg_check_modules(CLOUDPROVIDERS REQUIRED cloudproviders)
endif()

add_subdirectory("${DESKTOP_SOURCE_DIR}/shell_integration/libcloudproviders" cloudproviders)

string(TOLOWER "${APPLICATION_VENDOR}" dbus_vendor)
string(REGEX REPLACE "[^A-z0-9]" "" dbus_vendor "${dbus_vendor}")
string(REGEX REPLACE "[^A-z0-9]" "" dbus_application "${APPLICATION_SHORTNAME}")
set(bus_name "com.${dbus_vendor}.${dbus_application}")
set(object_path "/com/${dbus_vendor}/${dbus_application}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/identity" "${bus_name}\n${object_path}\n")

if(METADATA STREQUAL "desktop")
    set(LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS
        "Implements=org.freedesktop.CloudProviders\n\n[org.freedesktop.CloudProviders]\nBusName=${bus_name}\nObjectPath=${object_path}")
    configure_file("${DESKTOP_SOURCE_DIR}/mirall.desktop.in" nextcloud.desktop @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nextcloud.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
