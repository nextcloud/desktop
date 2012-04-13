# - Try to find Neon
# Once done this will define
#
#  NEON_FOUND - system has Neon
#  NEON_INCLUDE_DIRS - the Neon include directory
#  NEON_LIBRARIES - Link these to use Neon
#  NEON_DEFINITIONS - Compiler switches required for using Neon
#
#  Copyright (c) 2011 Andreas Schneider <asn@cryptomilk.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(_NEON neon)
endif (PKG_CONFIG_FOUND)

include(GNUInstallDirs)

find_path(NEON_INCLUDE_DIRS
NAMES
    neon/ne_basic.h
HINTS
    ${_NEON_INCLUDEDIR}
    ${CMAKE_INSTALL_INCLUDEDIR}
)

find_library(NEON_LIBRARIES
NAMES
    neon
HINTS
    ${_NEON_LIBDIR}
    ${CMAKE_INSTALL_LIBDIR}
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_INSTALL_PREFIX}/lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Neon DEFAULT_MSG NEON_LIBRARIES NEON_INCLUDE_DIRS)

# show the NEON_INCLUDE_DIRS and NEON_LIBRARIES variables only in the advanced view
mark_as_advanced(NEON_INCLUDE_DIRS NEON_LIBRARIES)
