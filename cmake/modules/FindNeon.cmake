# - Try to find Neon
# Once done this will define
#
#  NEON_FOUND - system has Neon
#  NEON_INCLUDE_DIRS - the Neon include directory
#  NEON_LIBRARIES - Link these to use Neon
#  NEON_DEFINITIONS - Compiler switches required for using Neon
#
#  Copyright (c) 2011-2013 Andreas Schneider <asn@cryptomilk.org>
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
    neon neon-27
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

# Check if neon was compiled with LFS support, if so, the NE_LFS variable has to
# be defined in the owncloud module.
# If neon was not compiled with LFS its also ok since the underlying system
# than probably supports large files anyway.
IF( CMAKE_FIND_ROOT_PATH )
	FIND_PROGRAM( NEON_CONFIG_EXECUTABLE NAMES neon-config HINTS ${CMAKE_FIND_ROOT_PATH}/bin )
ELSE( CMAKE_FIND_ROOT_PATH )
	FIND_PROGRAM( NEON_CONFIG_EXECUTABLE NAMES neon-config )
ENDIF( CMAKE_FIND_ROOT_PATH )

IF ( NEON_CONFIG_EXECUTABLE )
	MESSAGE(STATUS "neon-config executable: ${NEON_CONFIG_EXECUTABLE}")
	# neon-config --support lfs
	EXECUTE_PROCESS( COMMAND ${NEON_CONFIG_EXECUTABLE} "--support" "lfs"
			    RESULT_VARIABLE LFS
			    OUTPUT_STRIP_TRAILING_WHITESPACE )

	IF (LFS EQUAL 0)
		MESSAGE(STATUS "libneon has been compiled with LFS support")
		SET(NEON_WITH_LFS 1)
	ELSE (LFS EQUAL 0)
		MESSAGE(STATUS "libneon has not been compiled with LFS support, rely on OS")
	ENDIF (LFS EQUAL 0)
ELSE  ( NEON_CONFIG_EXECUTABLE )
    MESSAGE(STATUS, "neon-config could not be found.")
ENDIF ( NEON_CONFIG_EXECUTABLE )
