#
 # Copyright (C) by Camila Ayres <hello@camila.codes>
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation; either version 2 of the License, or
 # (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful, but
 # WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 # or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 # for more details.
 #

# Find the FUSE includes and library

#  DOKAN_INCLUDE_DIR - the DOKAN include directory
#  DOKAN_LIBRARIES - the DOKAN libraries

if(DOKAN_INCLUDE_DIR AND DOKAN_LIBRARIES)
    set(DOKAN_FOUND TRUE)
else()
    if(DEFINED ENV{DokanLibrary1})
        set(DOKAN_ROOT $ENV{DokanLibrary1})
    else()
        set(DOKAN_ROOT $ENV{PROGRAMFILES})
    endif()

    find_path(DOKAN_INCLUDE_DIR
                NAMES
                  dokan.h
                  fileinfo.h
                HINTS
                   ${DOKAN_ROOT}/include/dokan
                   ${DOKAN_ROOT}/Dokan/DokanLibraryDokanLibrary-1.1.0/include/dokan
                )

    find_library(DOKAN_LIBRARIES
                NAMES
                  dokan1
                  dokan
                PATHS
                  ${DOKAN_ROOT}/lib
                  ${DOKAN_ROOT}/Dokan/DokanLibraryDokanLibrary-1.1.0/lib
                )
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DOKAN_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Dokan DEFAULT_MSG
	DOKAN_LIBRARIES DOKAN_INCLUDE_DIR)

mark_as_advanced(DOKAN_INCLUDE_DIR DOKAN_LIBRARIES)