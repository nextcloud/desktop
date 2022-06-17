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

# FUNCTIONS
if (NOT LINUX)
    # librt
    check_library_exists(rt nanosleep "" HAVE_LIBRT)
endif (NOT LINUX)

check_function_exists(utimes HAVE_UTIMES)
check_function_exists(lstat HAVE_LSTAT)
