# - Try to find RT
# Once done this will define
#
#  RT_FOUND - system has RT
#  RT_INCLUDE_DIRS - the RT include directory
#  RT_LIBRARIES - Link these to use RT
#  RT_DEFINITIONS - Compiler switches required for using RT
#
#  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (RT_LIBRARIES AND RT_INCLUDE_DIRS)
  # in cache already
  set(RT_FOUND TRUE)
else (RT_LIBRARIES AND RT_INCLUDE_DIRS)
  find_path(RT_INCLUDE_DIR
    NAMES
      time.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(RT_LIBRARY
    NAMES
      rt
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (RT_LIBRARY)
    set(RT_FOUND TRUE)
  endif (RT_LIBRARY)

  set(RT_INCLUDE_DIRS
    ${RT_INCLUDE_DIR}
  )

  if (RT_FOUND)
    set(RT_LIBRARIES
      ${RT_LIBRARIES}
      ${RT_LIBRARY}
    )
  endif (RT_FOUND)

  if (RT_INCLUDE_DIRS AND RT_LIBRARIES)
     set(RT_FOUND TRUE)
  endif (RT_INCLUDE_DIRS AND RT_LIBRARIES)

  if (RT_FOUND)
    if (NOT RT_FIND_QUIETLY)
      message(STATUS "Found RT: ${RT_LIBRARIES}")
    endif (NOT RT_FIND_QUIETLY)
  else (RT_FOUND)
    if (RT_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find RT")
    endif (RT_FIND_REQUIRED)
  endif (RT_FOUND)

  # show the RT_INCLUDE_DIRS and RT_LIBRARIES variables only in the advanced view
  mark_as_advanced(RT_INCLUDE_DIRS RT_LIBRARIES)

endif (RT_LIBRARIES AND RT_INCLUDE_DIRS)

