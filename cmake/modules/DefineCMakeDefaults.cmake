# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# Always include srcdir and builddir in include path
# This saves typing ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY} in
# about every subdir
# since cmake 2.4.0
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Put the include dirs which are in the source or build tree
# before all other include dirs, so the headers in the sources
# are prefered over the already installed ones
# since cmake 2.4.1
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# Use colored output
# since cmake 2.4.0
set(CMAKE_COLOR_MAKEFILE ON)

# Define the generic version of the libraries here
set(GENERIC_LIB_VERSION "0.1.0")
set(GENERIC_LIB_SOVERSION "0")

# set -Werror
set(CMAKE_ENABLE_WERROR ON)

# Set the default build type to release with debug info
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo
    CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
  )
endif (NOT CMAKE_BUILD_TYPE)
