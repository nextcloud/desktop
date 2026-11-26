# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Older builds used either the project's datadir or the D-Bus pkg-config
# default, depending on the CMake version used to build them.
if(PKG_CONFIG_FOUND)
    pkg_get_variable(legacy_dbus_services_dir dbus-1 session_bus_services_dir)
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/RemoveLegacyService.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RemoveLegacyCloudProvidersService.cmake" @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/RemoveLegacyCloudProvidersService.cmake")
