# SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2021 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
target_sources(nextcloudCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.cpp
)

if( APPLE )
    target_sources(nextcloudCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/socketapi_mac.mm)
endif()
