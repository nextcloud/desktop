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


if (NEON_LIBRARIES AND NEON_INCLUDE_DIRS)
  # in cache already
  set(NEON_FOUND TRUE)
else (NEON_LIBRARIES AND NEON_INCLUDE_DIRS)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_NEON neon)
  endif (PKG_CONFIG_FOUND)

  find_path(NEON_INCLUDE_DIR
    NAMES
      neon/ne_basic.h
    PATHS
      ${_NEON_INCLUDEDIR}
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(NEON_LIBRARY
    NAMES
      neon
    PATHS
      ${_NEON_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  set(NEON_INCLUDE_DIRS
    ${NEON_INCLUDE_DIR}
  )

  if (NEON_LIBRARY)
    set(NEON_LIBRARIES
        ${NEON_LIBRARIES}
        ${NEON_LIBRARY}
    )
  endif (NEON_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Neon DEFAULT_MSG NEON_LIBRARIES NEON_INCLUDE_DIRS)

  # show the NEON_INCLUDE_DIRS and NEON_LIBRARIES variables only in the advanced view
  mark_as_advanced(NEON_INCLUDE_DIRS NEON_LIBRARIES)

endif (NEON_LIBRARIES AND NEON_INCLUDE_DIRS)
