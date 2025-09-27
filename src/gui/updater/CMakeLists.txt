# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    if (SPARKLE_FOUND)
        install(FILES ${CMAKE_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
    endif()
endif()

