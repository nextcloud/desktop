include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)

set(PACKAGE ${APPLICATION_NAME})
set(VERSION ${APPLICATION_VERSION})
set(DATADIR ${KDE_INSTALL_DATADIR})
set(LIBDIR ${LIB_INSTALL_DIR})
set(SYSCONFDIR ${CMAKE_INSTALL_SYSCONFDIR})

# FUNCTIONS
if (NOT LINUX)
    # librt
    check_library_exists(rt nanosleep "" HAVE_LIBRT)
endif (NOT LINUX)

check_function_exists(utimes HAVE_UTIMES)
check_function_exists(lstat HAVE_LSTAT)
