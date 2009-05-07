# - Try to find SQLite3
# Once done this will define
#
#  SQLITE3_FOUND - system has SQLite3
#  SQLITE3_INCLUDE_DIRS - the SQLite3 include directory
#  SQLITE3_LIBRARIES - Link these to use SQLite3
#  SQLITE3_DEFINITIONS - Compiler switches required for using SQLite3
#
#  Copyright (c) 2009 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (SQLITE3_LIBRARIES AND SQLITE3_INCLUDE_DIRS)
  # in cache already
  set(SQLITE3_FOUND TRUE)
else (SQLITE3_LIBRARIES AND SQLITE3_INCLUDE_DIRS)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  if (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    include(UsePkgConfig)
    pkgconfig(sqlite3 _SQLITE3_INCLUDEDIR _SQLITE3_LIBDIR _SQLITE3_LDFLAGS _SQLITE3_CFLAGS)
  else (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(_SQLITE3 sqlite3)
    endif (PKG_CONFIG_FOUND)
  endif (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4)

  find_path(SQLITE3_INCLUDE_DIR
    NAMES
      sqlite3.h
    PATHS
      ${_SQLITE3_INCLUDEDIR}
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )
  mark_as_advanced(SQLITE3_INCLUDE_DIR)

  find_library(SQLITE3_LIBRARY
    NAMES
      sqlite3
    PATHS
      ${_SQLITE3_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(SQLITE3_LIBRARY)

  if (SQLITE3_LIBRARY)
    set(SQLITE3_FOUND TRUE CACHE INTERNAL "Wether the sqlite3 library has been found" FORCE)
  endif (SQLITE3_LIBRARY)

  set(SQLITE3_INCLUDE_DIRS
    ${SQLITE3_INCLUDE_DIR}
  )

  if (SQLITE3_FOUND)
    set(SQLITE3_LIBRARIES
      ${SQLITE3_LIBRARIES}
      ${SQLITE3_LIBRARY}
    )
  endif (SQLITE3_FOUND)

  if (SQLITE3_INCLUDE_DIRS AND SQLITE3_LIBRARIES)
     set(SQLITE3_FOUND TRUE)
  endif (SQLITE3_INCLUDE_DIRS AND SQLITE3_LIBRARIES)

  if (SQLITE3_FOUND)
    if (NOT SQLite3_FIND_QUIETLY)
      message(STATUS "Found SQLite3: ${SQLITE3_LIBRARIES}")
    endif (NOT SQLite3_FIND_QUIETLY)
  else (SQLITE3_FOUND)
    if (SQLite3_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find SQLite3")
    endif (SQLite3_FIND_REQUIRED)
  endif (SQLITE3_FOUND)

  # show the SQLITE3_INCLUDE_DIRS and SQLITE3_LIBRARIES variables only in the advanced view
  mark_as_advanced(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)

endif (SQLITE3_LIBRARIES AND SQLITE3_INCLUDE_DIRS)

