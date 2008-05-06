# - Try to find Check
# Once done this will define
#
#  CHECK_FOUND - system has Check
#  CHECK_INCLUDE_DIRS - the Check include directory
#  CHECK_LIBRARIES - Link these to use Check
#  CHECK_DEFINITIONS - Compiler switches required for using Check
#
#  Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (CHECK_LIBRARIES AND CHECK_INCLUDE_DIRS)
  # in cache already
  set(CHECK_FOUND TRUE)
else (CHECK_LIBRARIES AND CHECK_INCLUDE_DIRS)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  if (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    include(UsePkgConfig)
    pkgconfig(check _CHECK_INCLUDEDIR _CHECK_LIBDIR _CHECK_LDFLAGS _CHECK_CFLAGS)
  else (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(_CHECK check)
    endif (PKG_CONFIG_FOUND)
  endif (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
  find_path(CHECK_INCLUDE_DIR
    NAMES
      check.h
    PATHS
      ${_CHECK_INCLUDEDIR}
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(CHECK_LIBRARY
    NAMES
      check
    PATHS
      ${_Check_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (CHECK_LIBRARY)
    set(CHECK_FOUND TRUE)
  endif (CHECK_LIBRARY)

  set(CHECK_INCLUDE_DIRS
    ${CHECK_INCLUDE_DIR}
  )

  if (CHECK_FOUND)
    set(CHECK_LIBRARIES
      ${CHECK_LIBRARY}
    )
  endif (CHECK_FOUND)

  if (CHECK_INCLUDE_DIRS AND CHECK_LIBRARIES)
     set(CHECK_FOUND TRUE)
  endif (CHECK_INCLUDE_DIRS AND CHECK_LIBRARIES)

  if (CHECK_FOUND)
    if (NOT Check_FIND_QUIETLY)
      message(STATUS "Found Check: ${CHECK_LIBRARIES}")
    endif (NOT Check_FIND_QUIETLY)
  else (CHECK_FOUND)
    if (Check_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find Check")
    endif (Check_FIND_REQUIRED)
  endif (CHECK_FOUND)

  # show the CHECK_INCLUDE_DIRS and CHECK_LIBRARIES variables only in the advanced view
  mark_as_advanced(CHECK_INCLUDE_DIRS CHECK_LIBRARIES)

endif (CHECK_LIBRARIES AND CHECK_INCLUDE_DIRS)

