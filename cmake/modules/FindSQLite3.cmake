# - Try to find SQLite3
# Once done this will define
#
#  SQLITE3_FOUND - system has SQLite3
#  SQLITE3_INCLUDE_DIRS - the SQLite3 include directory
#  SQLITE3_LIBRARIES - Link these to use SQLite3
#  SQLITE3_DEFINITIONS - Compiler switches required for using SQLite3
#
#  Copyright (c) 2009-2013 Andreas Schneider <asn@cryptomilk.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (UNIX)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_SQLITE3 sqlite3)
  endif (PKG_CONFIG_FOUND)
endif (UNIX)

find_path(SQLITE3_INCLUDE_DIR
    NAMES
        sqlite3.h
    PATHS
        ${_SQLITE3_INCLUDEDIR}
)

find_library(SQLITE3_LIBRARY
    NAMES
        sqlite3 sqlite3-0
    PATHS
        ${_SQLITE3_LIBDIR}
)

set(SQLITE3_INCLUDE_DIRS
    ${SQLITE3_INCLUDE_DIR}
)

if (SQLITE3_LIBRARY)
    set(SQLITE3_LIBRARIES
        ${SQLITE3_LIBRARIES}
        ${SQLITE3_LIBRARY}
    )
endif (SQLITE3_LIBRARY)

if (SQLite3_FIND_VERSION AND _SQLITE3_VERSION)
    set(SQLite3_VERSION _SQLITE3_VERSION)
endif (SQLite3_FIND_VERSION AND _SQLITE3_VERSION)

if (APPLE OR WIN32)
    set(USE_OUR_OWN_SQLITE3 TRUE)
    set(SQLITE3_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/3rdparty/sqlite3)
    set(SQLITE3_LIBRARIES "")
    set(SQLITE3_SOURCE ${SQLITE3_INCLUDE_DIR}/sqlite3.c)
    MESSAGE(STATUS "Using own sqlite3 from " ${SQLITE3_INCLUDE_DIR})
else()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(SQLite3 DEFAULT_MSG SQLITE3_LIBRARIES SQLITE3_INCLUDE_DIRS)
endif()



# show the SQLITE3_INCLUDE_DIRS and SQLITE3_LIBRARIES variables only in the advanced view
mark_as_advanced(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)

