# - Try to find Libsmbclient
# Once done this will define
#
#  LIBSMBCLIENT_FOUND - system has Libsmbclient
#  LIBSMBCLIENT_INCLUDE_DIRS - the Libsmbclient include directory
#  LIBSMBCLIENT_LIBRARIES - Link these to use Libsmbclient
#  LIBSMBCLIENT_DEFINITIONS - Compiler switches required for using Libsmbclient
#
#  Copyright (c) 2013 Andreas Schneider <asn@cryptomilk.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBSMBCLIENT_LIBRARIES AND LIBSMBCLIENT_INCLUDE_DIRS)
  # in cache already
  set(LIBSMBCLIENT_FOUND TRUE)
else (LIBSMBCLIENT_LIBRARIES AND LIBSMBCLIENT_INCLUDE_DIRS)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_LIBSMBCLIENT smbclient)
  endif (PKG_CONFIG_FOUND)

  find_path(LIBSMBCLIENT_INCLUDE_DIR
    NAMES
      libsmbclient.h
    PATHS
      ${_LIBSMBCLIENT_INCLUDEDIR}
    PATH_SUFFIXES
      samba-4.0
  )

  find_library(SMBCLIENT_LIBRARY
    NAMES
      smbclient
    PATHS
      ${_LIBSMBCLIENT_LIBDIR}
  )

  set(LIBSMBCLIENT_INCLUDE_DIRS
    ${LIBSMBCLIENT_INCLUDE_DIR}
  )

  if (SMBCLIENT_LIBRARY)
    set(LIBSMBCLIENT_LIBRARIES
        ${LIBSMBCLIENT_LIBRARIES}
        ${SMBCLIENT_LIBRARY}
    )
  endif (SMBCLIENT_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Libsmbclient DEFAULT_MSG LIBSMBCLIENT_LIBRARIES LIBSMBCLIENT_INCLUDE_DIRS)

  # show the LIBSMBCLIENT_INCLUDE_DIRS and LIBSMBCLIENT_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBSMBCLIENT_INCLUDE_DIRS LIBSMBCLIENT_LIBRARIES)

endif (LIBSMBCLIENT_LIBRARIES AND LIBSMBCLIENT_INCLUDE_DIRS)

