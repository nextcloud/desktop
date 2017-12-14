include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)

set(PACKAGE ${APPLICATION_NAME})
set(VERSION ${APPLICATION_VERSION})
set(DATADIR ${DATA_INSTALL_DIR})
set(LIBDIR ${LIB_INSTALL_DIR})
set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})

set(BINARYDIR ${CMAKE_CURRENT_BINARY_DIR})
set(SOURCEDIR ${CMAKE_CURRENT_SOURCE_DIR})

# HEADER FILES
check_include_file(argp.h HAVE_ARGP_H)

# FUNCTIONS
if (NOT LINUX)
    # librt
    check_library_exists(rt nanosleep "" HAVE_LIBRT)

    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} )
endif (NOT LINUX)

check_function_exists(asprintf HAVE_ASPRINTF)

check_function_exists(fnmatch HAVE_FNMATCH)
if(NOT HAVE_FNMATCH AND WIN32)
  find_library(SHLWAPI_LIBRARY shlwapi)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} shlwapi)
endif()

if(WIN32)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} psapi kernel32)
endif()

check_function_exists(timegm HAVE_TIMEGM)
check_function_exists(strerror_r HAVE_STRERROR_R)
check_function_exists(utimes HAVE_UTIMES)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(asprintf HAVE_ASPRINTF)
if (WIN32)
	check_function_exists(__mingw_asprintf HAVE___MINGW_ASPRINTF)
endif(WIN32)
if (UNIX AND HAVE_ASPRINTF)
  add_definitions(-D_GNU_SOURCE)
endif (UNIX AND HAVE_ASPRINTF)
if (WIN32)
  check_function_exists(__mingw_asprintf HAVE___MINGW_ASPRINTF)
endif(WIN32)

set(CSYNC_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "csync required system libraries")
