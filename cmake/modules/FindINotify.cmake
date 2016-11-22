# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# This module defines
#  INOTIFY_INCLUDE_DIR, where to find inotify.h, etc.
#  INOTIFY_LIBRARY_DIR, the directory holding the inotify library.
#  INOTIFY_FOUND, If false, do not try to use inotify.
# also defined, but not for general use are
#  INOTIFY_LIBRARY, where to find the inotify library.

find_path(INOTIFY_INCLUDE_DIR sys/inotify.h 
          PATH_SUFFIXES inotify)
mark_as_advanced(INOTIFY_INCLUDE_DIR)

find_library(INOTIFY_LIBRARY inotify PATH_SUFFIXES lib/inotify)

get_filename_component(INOTIFY_LIBRARY_DIR ${INOTIFY_LIBRARY} PATH)
mark_as_advanced(INOTIFY_LIBRARY_DIR)

# all listed variables are TRUE
# handle the QUIETLY and REQUIRED arguments and set INOTIFY_FOUND to TRUE if
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(INOTIFY DEFAULT_MSG INOTIFY_INCLUDE_DIR INOTIFY_LIBRARY_DIR)

IF(INOTIFY_FOUND)
  SET(INotify_INCLUDE_DIRS ${INOTIFY_INCLUDE_DIR})
  SET(INotify_LIBRARY_DIRS ${INOTIFY_LIBRARY_DIR})
ENDIF(INOTIFY_FOUND)

