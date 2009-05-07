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
  mark_as_advanced(LOG4C_INCLUDE_DIR)

  find_library(LOG4C_LIBRARY
    NAMES
      log4c
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(LOG4C_LIBRARY)

  if (LOG4C_LIBRARY)
    set(LOG4C_FOUND TRUE CACHE INTERNAL "Wether the log4c library has been found" FORCE)
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
    if (NOT Log4C_FIND_QUIETLY)
      message(STATUS "Found Log4C: ${LOG4C_LIBRARIES}")
    endif (NOT Log4C_FIND_QUIETLY)
  else (LOG4C_FOUND)
    if (Log4C_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find Log4C")
    endif (Log4C_FIND_REQUIRED)
  endif (LOG4C_FOUND)

  # show the LOG4C_INCLUDE_DIRS and LOG4C_LIBRARIES variables only in the advanced view
  mark_as_advanced(LOG4C_INCLUDE_DIRS LOG4C_LIBRARIES)

endif (LOG4C_LIBRARIES AND LOG4C_INCLUDE_DIRS)

