# MSI package scripts, helper DLL and migration tools
# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
if(BUILD_WIN_MSI)
    add_subdirectory(msi)
endif()

if(BUILD_WIN_MSI OR BUILD_WIN_TOOLS)
    add_subdirectory(tools)
endif()
