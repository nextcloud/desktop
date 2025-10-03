# - Try to find SMBClient
# Once done this will define
#
#  SMBCLIENT_FOUND - system has SMBClient
#  SMBCLIENT_INCLUDE_DIRS - the SMBClient include directory
#  SMBCLIENT_LIBRARIES - Link these to use SMBClient
#  SMBCLIENT_DEFINITIONS - Compiler switches required for using SMBClient
#
#  Copyright (c) 2013 Andreas Schneider <asn@cryptomilk.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (SMBCLIENT_LIBRARIES AND SMBCLIENT_INCLUDE_DIRS)
  # in cache already
  set(SMBCLIENT_FOUND TRUE)
else (SMBCLIENT_LIBRARIES AND SMBCLIENT_INCLUDE_DIRS)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_SMBCLIENT smbclient)
  endif (PKG_CONFIG_FOUND)

  find_path(SMBCLIENT_INCLUDE_DIR
    NAMES
      libsmbclient.h
    PATHS
      ${_SMBCLIENT_INCLUDEDIR}
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(SMBCLIENT_LIBRARY
    NAMES
      smbclient
    PATHS
      ${_SMBCLIENT_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  set(SMBCLIENT_INCLUDE_DIRS
    ${SMBCLIENT_INCLUDE_DIR}
  )

  if (SMBCLIENT_LIBRARY)
    set(SMBCLIENT_LIBRARIES
        ${SMBCLIENT_LIBRARIES}
        ${SMBCLIENT_LIBRARY}
    )
  endif (SMBCLIENT_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SMBClient DEFAULT_MSG SMBCLIENT_LIBRARIES SMBCLIENT_INCLUDE_DIRS)

  # show the SMBCLIENT_INCLUDE_DIRS and SMBCLIENT_LIBRARIES variables only in the advanced view
  mark_as_advanced(SMBCLIENT_INCLUDE_DIRS SMBCLIENT_LIBRARIES)

endif (SMBCLIENT_LIBRARIES AND SMBCLIENT_INCLUDE_DIRS)

