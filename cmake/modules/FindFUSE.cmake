# Find the FUSE includes and library
#
# Once done this will define
#  FUSE_FOUND - system has FUSE
#  FUSE_INCLUDE_DIR - the FUSE include directory
#  FUSE_LIBRARIES - List of libraries when using FUSE.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.

# check if already in cache, be silent
IF (FUSE_INCLUDE_DIR)
        SET (FUSE_FIND_QUIETLY TRUE)
ENDIF (FUSE_INCLUDE_DIR)

# find includes
FIND_PATH (FUSE_INCLUDE_DIR fuse.h
        /usr/local/include/osxfuse
        /usr/local/include
        /usr/include
)

# find lib
if (APPLE)
    SET(FUSE_NAMES libosxfuse.dylib fuse)
else (APPLE)
    SET(FUSE_NAMES fuse)
endif (APPLE)

FIND_LIBRARY(FUSE_LIBRARIES
        NAMES ${FUSE_NAMES}
        PATHS /lib64 /lib /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(FUSE DEFAULT_MSG FUSE_INCLUDE_DIR FUSE_LIBRARIES)
mark_as_advanced(FUSE_INCLUDE_DIR FUSE_LIBRARIES)

