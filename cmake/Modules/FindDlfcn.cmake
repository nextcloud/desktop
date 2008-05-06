# - Try to find Dlfcn
# Once done this will define
#
#  DLFCN_FOUND - system has Dlfcn
#  DLFCN_INCLUDE_DIRS - the Dlfcn include directory
#  DLFCN_LIBRARIES - Link these to use Dlfcn
#  DLFCN_DEFINITIONS - Compiler switches required for using Dlfcn
#
#  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (DLFCN_LIBRARIES AND DLFCN_INCLUDE_DIRS)
  # in cache already
  set(DLFCN_FOUND TRUE)
else (DLFCN_LIBRARIES AND DLFCN_INCLUDE_DIRS)
  find_path(DLFCN_INCLUDE_DIR
    NAMES
      dlfcn.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(DL_LIBRARY
    NAMES
      dl
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (DL_LIBRARY)
    set(DL_FOUND TRUE)
  endif (DL_LIBRARY)

  set(DLFCN_INCLUDE_DIRS
    ${DLFCN_INCLUDE_DIR}
  )

  if (DL_FOUND)
    set(DLFCN_LIBRARIES
      ${DLFCN_LIBRARIES}
      ${DL_LIBRARY}
    )
  endif (DL_FOUND)

  if (DLFCN_INCLUDE_DIRS AND DLFCN_LIBRARIES)
     set(DLFCN_FOUND TRUE)
  endif (DLFCN_INCLUDE_DIRS AND DLFCN_LIBRARIES)

  if (DLFCN_FOUND)
    if (NOT Dlfcn_FIND_QUIETLY)
      message(STATUS "Found Dlfcn: ${DLFCN_LIBRARIES}")
    endif (NOT Dlfcn_FIND_QUIETLY)
  else (DLFCN_FOUND)
    if (Dlfcn_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find Dlfcn")
    endif (Dlfcn_FIND_REQUIRED)
  endif (DLFCN_FOUND)

  # show the DLFCN_INCLUDE_DIRS and DLFCN_LIBRARIES variables only in the advanced view
  mark_as_advanced(DLFCN_INCLUDE_DIRS DLFCN_LIBRARIES)

endif (DLFCN_LIBRARIES AND DLFCN_INCLUDE_DIRS)

