# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
project(NCNavRemove)

set(MUTEX_NAME "NCNavRemove")

configure_file(NavRemoveConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/NavRemoveConstants.h)
configure_file(NavRemove.ini.in ${CMAKE_CURRENT_BINARY_DIR}/NavRemove.ini)
configure_file(version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

add_subdirectory(dll)
add_subdirectory(exe)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/NavRemove.ini
    DESTINATION tools/NCNavRemove/
)
