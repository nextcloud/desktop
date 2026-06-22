# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud, Inc.
# SPDX-License-Identifier: GPL-2.0-or-later
if(APPLE)
    # traverse into osx subdirectory to install and patch the create-pack script
    add_subdirectory(osx)
elseif(WIN32)
    # MSI package scripts, helper DLL and migration tools
    add_subdirectory(win)
endif()
