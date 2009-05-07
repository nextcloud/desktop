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
  mark_as_advanced(LIBSSH_INCLUDE_DIR)

  find_library(SSH_LIBRARY
    NAMES
      ssh
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(SSH_LIBRARY)

  if (SSH_LIBRARY)
    set(SSH_FOUND TRUE CACHE INTERNAL "Wether the ssh library has been found" FORCE)
  endif (SSH_LIBRARY)

  set(LIBSSH_INCLUDE_DIRS
    ${LIBSSH_INCLUDE_DIR}
  )

  if (SSH_FOUND)
    set(LIBSSH_LIBRARIES
      ${LIBSSH_LIBRARIES}
      ${SSH_LIBRARY}
    )
  endif (SSH_FOUND)

  if (LIBSSH_INCLUDE_DIRS AND LIBSSH_LIBRARIES)
     set(LIBSSH_FOUND TRUE)
  endif (LIBSSH_INCLUDE_DIRS AND LIBSSH_LIBRARIES)

  if (LIBSSH_FOUND)
    if (NOT LibSSH_FIND_QUIETLY)
      message(STATUS "Found LibSSH: ${LIBSSH_LIBRARIES}")
    endif (NOT LibSSH_FIND_QUIETLY)
  else (LIBSSH_FOUND)
    if (LibSSH_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find LibSSH")
    endif (LibSSH_FIND_REQUIRED)
  endif (LIBSSH_FOUND)

  # show the LIBSSH_INCLUDE_DIRS and LIBSSH_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBSSH_INCLUDE_DIRS LIBSSH_LIBRARIES)

endif (LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)

