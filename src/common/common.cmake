# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2017 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Just list files to build as part of the csync dynamic lib.
# Essentially they could be in the same directory but are separate to
# help keep track of the different code licenses.
set(common_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/checksums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/checksumcalculator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/filesystembase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ownsql.cpp
    ${CMAKE_CURRENT_LIST_DIR}/preparedsqlquerymanager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournaldb.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournalfilerecord.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utility.cpp
    ${CMAKE_CURRENT_LIST_DIR}/remotepermissions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vfs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pinstate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncfilestatus.cpp
)

if(WIN32)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_win.cpp
    )
elseif(APPLE)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_mac.mm
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_unix.cpp
    )
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/vfspluginmetadata.json.in ${CMAKE_CURRENT_BINARY_DIR}/vfspluginmetadata.json)
