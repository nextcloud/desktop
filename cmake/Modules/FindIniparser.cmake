# - Try to find Iniparser
# Once done this will define
#
#  INIPARSER_FOUND - system has Iniparser
#  INIPARSER_INCLUDE_DIRS - the Iniparser include directory
#  INIPARSER_LIBRARIES - Link these to use Iniparser
#  INIPARSER_DEFINITIONS - Compiler switches required for using Iniparser
#
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (INIPARSER_LIBRARIES AND INIPARSER_INCLUDE_DIRS)
  # in cache already
  set(INIPARSER_FOUND TRUE)
else (INIPARSER_LIBRARIES AND INIPARSER_INCLUDE_DIRS)
  find_path(INIPARSER_INCLUDE_DIR
    NAMES
      iniparser.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(INIPARSER_LIBRARY
    NAMES
      iniparser
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (INIPARSER_LIBRARY)
    set(INIPARSER_FOUND TRUE)
  endif (INIPARSER_LIBRARY)

  set(INIPARSER_INCLUDE_DIRS
    ${INIPARSER_INCLUDE_DIR}
  )

  if (INIPARSER_FOUND)
    set(INIPARSER_LIBRARIES
      ${INIPARSER_LIBRARIES}
      ${INIPARSER_LIBRARY}
    )
  endif (INIPARSER_FOUND)

  if (INIPARSER_INCLUDE_DIRS AND INIPARSER_LIBRARIES)
     set(INIPARSER_FOUND TRUE)
  endif (INIPARSER_INCLUDE_DIRS AND INIPARSER_LIBRARIES)

  if (INIPARSER_FOUND)
    if (NOT Iniparser_FIND_QUIETLY)
      message(STATUS "Found Iniparser: ${INIPARSER_LIBRARIES}")
    endif (NOT Iniparser_FIND_QUIETLY)
  else (INIPARSER_FOUND)
    if (Iniparser_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find Iniparser")
    endif (Iniparser_FIND_REQUIRED)
  endif (INIPARSER_FOUND)

  # show the INIPARSER_INCLUDE_DIRS and INIPARSER_LIBRARIES variables only in the advanced view
  mark_as_advanced(INIPARSER_INCLUDE_DIRS INIPARSER_LIBRARIES)

endif (INIPARSER_LIBRARIES AND INIPARSER_INCLUDE_DIRS)

