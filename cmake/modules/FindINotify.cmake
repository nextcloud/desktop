# (c) 2014 Copyright ownCloud, Inc.
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# This module defines
#  INOTIFY_INCLUDE_DIR, where to find inotify.h, etc.
#  INOTIFY_FOUND, If false, do not try to use inotify.
# also defined, but not for general use are
#  INOTIFY_LIBRARY, where to find the inotify library.

find_path(INOTIFY_INCLUDE_DIR sys/inotify.h 
          HINTS /usr/include/${CMAKE_LIBRARY_ARCHITECTURE})
mark_as_advanced(INOTIFY_INCLUDE_DIR)

# all listed variables are TRUE
# handle the QUIETLY and REQUIRED arguments and set INOTIFY_FOUND to TRUE if
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(INOTIFY DEFAULT_MSG INOTIFY_INCLUDE_DIR)

IF(INOTIFY_FOUND)
  SET(INotify_INCLUDE_DIRS ${INOTIFY_INCLUDE_DIR})
ENDIF(INOTIFY_FOUND)

