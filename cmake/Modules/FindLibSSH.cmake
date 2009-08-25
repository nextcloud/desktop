# - Try to find LibSSH
# Once done this will define
#
#  LIBSSH_FOUND - system has LibSSH
#  LIBSSH_INCLUDE_DIRS - the LibSSH include directory
#  LIBSSH_LIBRARIES - Link these to use LibSSH
#  LIBSSH_DEFINITIONS - Compiler switches required for using LibSSH
#
#  Copyright (c) 2009 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)
  # in cache already
  set(LIBSSH_FOUND TRUE)
else (LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)

  find_path(LIBSSH_INCLUDE_DIR
    NAMES
      libssh/libssh.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(SSH_LIBRARY
    NAMES
      ssh
      libssh
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (SSH_LIBRARY)
    set(SSH_FOUND TRUE)
  endif (SSH_LIBRARY)

  set(LIBSSH_INCLUDE_DIRS
    ${LIBSSH_INCLUDE_DIR}
  )

  if (SSH_FOUND)
    set(LIBSSH_LIBRARIES
      ${LIBSSH_LIBRARIES}
      ${SSH_LIBRARY}
    )

    if (LibSSH_FIND_VERSION)
      file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_MAJOR
        REGEX "#define[ ]+LIBSSH_VERSION_MAJOR[ ]+[0-9]+")
      string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MAJOR ${LIBSSH_VERSION_MAJOR})
      file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_MINOR
        REGEX "#define[ ]+LIBSSH_VERSION_MINOR[ ]+[0-9]+")
      string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MINOR ${LIBSSH_VERSION_MINOR})
      file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_PATCH
        REGEX "#define[ ]+LIBSSH_VERSION_MICRO[ ]+[0-9]+")
      string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_PATCH ${LIBSSH_VERSION_PATCH})

      set(LibSSH_VERSION ${LIBSSH_VERSION_MAJOR}.${LIBSSH_VERSION_MINOR}.${LIBSSH_VERSION_PATCH})

      include(FindPackageVersionCheck)
      find_package_version_check(LibSSH DEFAULT_MSG)
    endif (LibSSH_FIND_VERSION)
  endif (SSH_FOUND)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(LibSSH DEFAULT_MSG LIBSSH_LIBRARIES LIBSSH_INCLUDE_DIRS)

  # show the LIBSSH_INCLUDE_DIRS and LIBSSH_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES)

endif (LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)

