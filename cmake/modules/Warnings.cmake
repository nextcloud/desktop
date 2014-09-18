# (c) 2014 Copyright ownCloud, Inc.
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

if(CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)
    if(GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-long-long")
    else(GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-long-long")
    endif(GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif(CMAKE_COMPILER_IS_GNUCXX)
if(CMAKE_CXX_COMPILER MATCHES "clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-long-long")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif(CMAKE_CXX_COMPILER MATCHES "clang")
# TODO: handle msvc compilers warnings?

if(DEFINED MIRALL_FATAL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER MATCHES "clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER MATCHES "clang")
    # TODO: handle msvc compilers warnings?
endif(DEFINED MIRALL_FATAL_WARNINGS)
