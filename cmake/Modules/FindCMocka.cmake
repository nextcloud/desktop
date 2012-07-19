# - Try to find CMocka, the testing framework
# Once done this will define
#
#  CMOCKA_FOUND - system has CMocka
#  CMOCKA_INCLUDE_DIRS - the CMocka include directory
#  CMOCKA_LIBRARIES - Link these to use CMocka
#  CMOCKA_DEFINITIONS - Compiler switches required for using CMocka
#
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#            (c) 2012 Dominik Schmidt <dev@dominik-schmidt.org>
#            (c) 2012 Klaas Freitag <freitag@owncloud.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

include(GNUInstallDirs)

find_path(CMOCKA_INCLUDE_DIRS
    NAMES
        cmocka.h
    HINTS
        ${CMAKE_INSTALL_INCLUDEDIR}
)

find_library(CMOCKA_LIBRARIES
    NAMES
        cmocka
    HINTS
        ${CMAKE_INSTALL_PREFIX}/lib
        ${CMAKE_INSTALL_PREFIX}/lib64
        ${CMAKE_INSTALL_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cmocka
    REQUIRED_VARS CMOCKA_LIBRARIES CMOCKA_INCLUDE_DIRS
)


