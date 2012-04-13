# - Try to find Iniparser
# Once done this will define
#
#  INIPARSER_FOUND - system has Iniparser
#  INIPARSER_INCLUDE_DIRS - the Iniparser include directory
#  INIPARSER_LIBRARIES - Link these to use Iniparser
#  INIPARSER_DEFINITIONS - Compiler switches required for using Iniparser
#
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#            (c) 2012 Dominik Schmidt <dev@dominik-schmidt.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

include(GNUInstallDirs)

find_path(INIPARSER_INCLUDE_DIRS
    NAMES
        iniparser.h
    HINTS
        ${CMAKE_INSTALL_INCLUDEDIR}
)

find_library(INIPARSER_LIBRARIES
    NAMES
        iniparser
    PATHS
        ${CMAKE_INSTALL_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Iniparser
    REQUIRED_VARS INIPARSER_LIBRARIES INIPARSER_INCLUDE_DIRS
)


