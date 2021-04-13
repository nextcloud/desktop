#.rst:
# FindInotify
# --------------
#
# Try to find inotify on this system. This finds:
#  - libinotify on Unix like systems, or
#  - the kernel's inotify on Linux systems.
#
# This will define the following variables:
#
# ``Inotify_FOUND``
#    True if inotify is available
# ``Inotify_LIBRARIES``
#    This has to be passed to target_link_libraries()
# ``Inotify_INCLUDE_DIRS``
#    This has to be passed to target_include_directories()
#
# On Linux, the libraries and include directories are empty,
# even though ``Inotify_FOUND`` may be set to TRUE. This is because
# no special includes or libraries are needed. On other systems
# these may be needed to use inotify.
#
# Since 5.32.0.

#=============================================================================
# SPDX-FileCopyrightText: 2016 Tobias C. Berner <tcberner@FreeBSD.org>
# SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#=============================================================================

find_path(Inotify_INCLUDE_DIRS sys/inotify.h)

if(Inotify_INCLUDE_DIRS)
# On Linux there is no library to link against, on the BSDs there is.
# On the BSD's, inotify is implemented through a library, libinotify.
    if( CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(Inotify_FOUND TRUE)
        set(Inotify_LIBRARIES "")
        set(Inotify_INCLUDE_DIRS "")
    else()
        find_library(Inotify_LIBRARIES NAMES inotify)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(Inotify
            FOUND_VAR
                Inotify_FOUND
            REQUIRED_VARS
                Inotify_LIBRARIES
                Inotify_INCLUDE_DIRS
        )
        mark_as_advanced(Inotify_LIBRARIES Inotify_INCLUDE_DIRS)
        include(FeatureSummary)
        set_package_properties(Inotify PROPERTIES
            URL "https://github.com/libinotify-kqueue/"
            DESCRIPTION "inotify API on the *BSD family of operating systems."
        )
    endif()
else()
   set(Inotify_FOUND FALSE)
endif()

mark_as_advanced(Inotify_LIBRARIES Inotify_INCLUDE_DIRS)
