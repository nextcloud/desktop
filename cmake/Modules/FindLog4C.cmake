# - Try to find Log4C
# Once done this will define
#
#  LOG4C_FOUND - system has Log4C
#  LOG4C_INCLUDE_DIRS - the Log4C include directory
#  LOG4C_LIBRARIES - Link these to use Log4C
#  LOG4C_DEFINITIONS - Compiler switches required for using Log4C
#
#  Copyright (c) 2009 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

include(GNUInstallDirs)

find_path(LOG4C_INCLUDE_DIRS
    NAMES
      log4c.h
    HINTS
        ${CMAKE_INSTALL_INCLUDEDIR}
)

find_library(LOG4C_LIBRARIES
    NAMES
        log4c
    HINTS
        ${CMAKE_INSTALL_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Log4C DEFAULT_MSG LOG4C_LIBRARIES LOG4C_INCLUDE_DIRS)

# show the LOG4C_INCLUDE_DIRS and LOG4C_LIBRARIES variables only in the advanced view
mark_as_advanced(LOG4C_INCLUDE_DIRS LOG4C_LIBRARIES)
