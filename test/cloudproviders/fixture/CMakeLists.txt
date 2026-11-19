# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)
project(CloudProvidersInstallFixture LANGUAGES NONE)
include("${DESKTOP_SOURCE_DIR}/NEXTCLOUD.cmake")
string(TOLOWER "${APPLICATION_VENDOR}" dbus_vendor)
string(REGEX REPLACE "[^A-z0-9]" "" dbus_vendor "${dbus_vendor}")
string(REGEX REPLACE "[^A-z0-9]" "" dbus_application "${APPLICATION_SHORTNAME}")
set(bus_name "com.${dbus_vendor}.${dbus_application}")
set(object_path "/com/${dbus_vendor}/${dbus_application}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/identity" "${bus_name}\n${object_path}\n")

set(LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS
    "Implements=org.freedesktop.CloudProviders\n\n[org.freedesktop.CloudProviders]\nBusName=${bus_name}\nObjectPath=${object_path}")
configure_file("${DESKTOP_SOURCE_DIR}/mirall.desktop.in" nextcloud.desktop @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nextcloud.desktop" DESTINATION "share/applications")
