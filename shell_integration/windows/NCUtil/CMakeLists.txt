# SPDX-FileCopyrightText: 2018 ownCloud GmbH
# SPDX-License-Identifier: LGPL-2.1-or-later
configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h)

add_library(NCUtil STATIC
    CommunicationSocket.cpp
    RemotePathChecker.cpp
    StringUtil.cpp
    NCUtil.rc
)

target_include_directories(NCUtil
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
