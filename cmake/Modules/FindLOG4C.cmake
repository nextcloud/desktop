# - Try to find LOG4C
# Once done this will define
#
#  LOG4C_FOUND - system has LOG4C
#  LOG4C_INCLUDE_DIRS - the LOG4C include directory
#  LOG4C_LIBRARIES - Link these to use LOG4C
#  LOG4C_DEFINITIONS - Compiler switches required for using LOG4C
#
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LOG4C_LIBRARIES AND LOG4C_INCLUDE_DIRS)
  # in cache already
  set(LOG4C_FOUND TRUE)
else (LOG4C_LIBRARIES AND LOG4C_INCLUDE_DIRS)
  find_path(LOG4C_INCLUDE_DIR
    NAMES
      log4c.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(LOG4C_LIBRARY
    NAMES
      log4c
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (LOG4C_LIBRARY)
    set(LOG4C_FOUND TRUE)
  endif (LOG4C_LIBRARY)

  set(LOG4C_INCLUDE_DIRS
    ${LOG4C_INCLUDE_DIR}
  )

  if (LOG4C_FOUND)
    set(LOG4C_LIBRARIES
      ${LOG4C_LIBRARIES}
      ${LOG4C_LIBRARY}
    )
  endif (LOG4C_FOUND)

  if (LOG4C_INCLUDE_DIRS AND LOG4C_LIBRARIES)
     set(LOG4C_FOUND TRUE)
  endif (LOG4C_INCLUDE_DIRS AND LOG4C_LIBRARIES)

  if (LOG4C_FOUND)
    if (NOT LOG4C_FIND_QUIETLY)
      message(STATUS "Found LOG4C: ${LOG4C_LIBRARIES}")
    endif (NOT LOG4C_FIND_QUIETLY)
  else (LOG4C_FOUND)
    if (LOG4C_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find LOG4C")
    endif (LOG4C_FIND_REQUIRED)
  endif (LOG4C_FOUND)

  # show the LOG4C_INCLUDE_DIRS and LOG4C_LIBRARIES variables only in the advanced view
  mark_as_advanced(LOG4C_INCLUDE_DIRS LOG4C_LIBRARIES)

endif (LOG4C_LIBRARIES AND LOG4C_INCLUDE_DIRS)

