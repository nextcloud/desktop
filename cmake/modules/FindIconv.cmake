# - Try to find Iconv
# Once done this will define
#
#  ICONV_FOUND - system has Iconv
#  ICONV_INCLUDE_DIRS - the Iconv include directory
#  ICONV_LIBRARIES - Link these to use Iconv
#  ICONV_DEFINITIONS - Compiler switches required for using Iconv
#
#  Copyright (c) 2013 Andreas Schneider <asn@cryptomilk.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckPrototypeDefinition)

find_path(ICONV_INCLUDE_DIR
    NAMES
        iconv.h sys/iconv.h
)

set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
check_include_file(iconv.h HAVE_ICONV_H)
check_include_file(sys/iconv.h HAVE_SYS_ICONV_H)
set(CMAKE_REQUIRED_INCLUDES)

find_library(ICONV_LIBRARY
    NAMES
        iconv
        libiconv
    PATHS
)

if (ICONV_LIBRARY)
    get_filename_component(_ICONV_NAME ${ICONV_LIBRARY} NAME)
    get_filename_component(_ICONV_PATH ${ICONV_LIBRARY} PATH)
    check_library_exists(${_ICONV_NAME} iconv ${_ICONV_PATH} HAVE_ICONV)
else()
    check_function_exists(iconv HAVE_ICONV)
endif()

if (HAVE_ICONV_H OR HAVE_SYS_ICONV_H)
    if (HAVE_ICONV_H)
        set(_ICONV_PROTO_INCLUDE "iconv.h")
    endif (HAVE_ICONV_H)
    if (HAVE_SYS_ICONV_H)
        set(_ICONV_PROTO_INCLUDE "sys/iconv.h")
    endif (HAVE_SYS_ICONV_H)

    set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
    check_prototype_definition(iconv
        "size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)"
        "-1"
        ${_ICONV_PROTO_INCLUDE}
        HAVE_ICONV_CONST)
    set(CMAKE_REQUIRED_INCLUDES)
endif (HAVE_ICONV_H OR HAVE_SYS_ICONV_H)

set(ICONV_INCLUDE_DIRS
    ${ICONV_INCLUDE_DIR}
)

if (ICONV_LIBRARY)
    set(ICONV_LIBRARIES
        ${ICONV_LIBRARIES}
        ${ICONV_LIBRARY}
    )
endif (ICONV_LIBRARY)

include(FindPackageHandleStandardArgs)
if (ICONV_LIBRARIES)
    find_package_handle_standard_args(Iconv DEFAULT_MSG ICONV_LIBRARIES ICONV_INCLUDE_DIRS)
else()
    find_package_handle_standard_args(Iconv DEFAULT_MSG ICONV_INCLUDE_DIRS)
endif()

# show the ICONV_INCLUDE_DIRS and ICONV_LIBRARIES variables only in the advanced view
mark_as_advanced(ICONV_INCLUDE_DIRS ICONV_LIBRARIES)
