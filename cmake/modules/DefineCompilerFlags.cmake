# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# define system dependent compiler flags

include(CheckCCompilerFlag)
include(CheckCCompilerFlagSSP)

#
# Define GNUCC compiler flags
#
if (${CMAKE_C_COMPILER_ID} MATCHES "(GNU|Clang)")

    # add -Wconversion ?
    # cannot be pedantic with sqlite3 directly linked
    # FIXME Can we somehow not use those flags for sqlite3.* but use them for the rest of csync?
    if (NOT USE_OUR_OWN_SQLITE3)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -pedantic -pedantic-errors")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wshadow -Wmissing-prototypes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused -Wfloat-equal -Wpointer-arith -Wwrite-strings -Wformat-security")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-format-attribute")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-format-attribute -D_GNU_SOURCE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_FORMAT_MACROS=1")

    set(CSYNC_STRICT OFF CACHE BOOL "Strict error checking, enabled -Werror and friends")
    if (CSYNC_STRICT)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    endif(CSYNC_STRICT)

    # with -fPIC
    check_c_compiler_flag("-fPIC" WITH_FPIC)
    if (WITH_FPIC AND NOT WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    endif (WITH_FPIC AND NOT WIN32)

    check_c_compiler_flag_ssp("-fstack-protector" WITH_STACK_PROTECTOR)
    if (WITH_STACK_PROTECTOR AND NOT WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
    endif (WITH_STACK_PROTECTOR AND NOT WIN32)

    if (CMAKE_BUILD_TYPE)
        string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
        if (CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)")
            check_c_compiler_flag("-Wp,-D_FORTIFY_SOURCE=2" WITH_FORTIFY_SOURCE)
            if (WITH_FORTIFY_SOURCE)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wp,-D_FORTIFY_SOURCE=2")
            endif (WITH_FORTIFY_SOURCE)
        endif()
    endif()
endif (${CMAKE_C_COMPILER_ID} MATCHES "(GNU|Clang)")

if (UNIX AND NOT WIN32)
    #
    # Check for large filesystem support
    #
    if (CMAKE_SIZEOF_VOID_P MATCHES "8")
        # with large file support
        execute_process(
            COMMAND
                getconf LFS64_CFLAGS
            OUTPUT_VARIABLE
                _lfs_CFLAGS
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else (CMAKE_SIZEOF_VOID_P MATCHES "8")
        # with large file support
        execute_process(
            COMMAND
                getconf LFS_CFLAGS
            OUTPUT_VARIABLE
                _lfs_CFLAGS
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif (CMAKE_SIZEOF_VOID_P MATCHES "8")
    if (_lfs_CFLAGS)
        string(REGEX REPLACE "[\r\n]" " " "${_lfs_CFLAGS}" "${${_lfs_CFLAGS}}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_lfs_CFLAGS}")
    endif (_lfs_CFLAGS)
else(UNIX AND NOT WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
endif (UNIX AND NOT WIN32)

if (MSVC)
    # Use secure functions by default and suppress warnings about
    #"deprecated" functions
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _CRT_NONSTDC_NO_WARNINGS=1 /D _CRT_SECURE_NO_WARNINGS=1")
endif (MSVC)
