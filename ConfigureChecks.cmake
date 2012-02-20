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
set(PLUGINDIR "${PLUGIN_INSTALL_DIR}-${LIBRARY_SOVERSION}")
set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})

set(BINARYDIR ${CMAKE_BINARY_DIR})
set(SOURCEDIR ${CMAKE_SOURCE_DIR})

# HEADER FILES
check_include_file(argp.h HAVE_ARGP_H)

# FUNCTIONS
if (NOT LINUX)
    # librt
    check_library_exists(rt nanosleep "" HAVE_LIBRT)

    # argp
    find_package(Argp REQUIRED)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${ARGP_LIBRARY})
endif (NOT LINUX)

check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME)
if (HAVE_LIBRT OR HAVE_CLOCK_GETTIME)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} rt)
endif (HAVE_LIBRT OR HAVE_CLOCK_GETTIME)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if (HAVE_LIBDL)
    find_library(DLFCN_LIBRARY dl)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${DLFCN_LIBRARY})
endif (HAVE_LIBDL)

check_function_exists(strerror_r HAVE_STRERROR_R)

set(CSYNC_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} CACHE INTERNAL "csync required system libraries")
