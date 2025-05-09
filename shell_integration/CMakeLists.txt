# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2012 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
if (APPLE)
  add_subdirectory(MacOSX)
endif()
if(BUILD_SHELL_INTEGRATION_ICONS)
  add_subdirectory(icons)
endif()

if( UNIX AND NOT APPLE )
  if(BUILD_SHELL_INTEGRATION_NAUTILUS)
    add_subdirectory(nautilus)
  endif()

  if(BUILD_SHELL_INTEGRATION_DOLPHIN)
    find_package(KF5KIO "5.16" CONFIG QUIET)
    find_package(KF6KIO "5.240" CONFIG QUIET)
    if(KF5KIO_FOUND OR KF6KIO_FOUND)
      add_subdirectory(dolphin)
    else()
      message("Dolphin plugin disabled: KDE Frameworks 5 and 6 not found")
    endif()
  endif()
endif()

if(CLOUDPROVIDERS_FOUND)
  add_subdirectory(libcloudproviders)
endif()

if(MSVC)
  add_subdirectory(windows)
endif()
