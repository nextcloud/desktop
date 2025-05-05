# SPDX-FileCopyrightText: 2018 ownCloud GmbH
# SPDX-License-Identifier: LGPL-2.1-or-later
add_library(NCUtil STATIC
    CommunicationSocket.cpp
    RemotePathChecker.cpp
    StringUtil.cpp
    NCUtil.rc
)

target_include_directories(NCUtil
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
