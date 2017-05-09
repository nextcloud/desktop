# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# - Try to find QtKeychain
# Once done this will define
#  QTKEYCHAIN_FOUND - System has QtKeychain
#  QTKEYCHAIN_INCLUDE_DIRS - The QtKeychain include directories
#  QTKEYCHAIN_LIBRARIES - The libraries needed to use QtKeychain
#  QTKEYCHAIN_DEFINITIONS - Compiler switches required for using LibXml2

# When we build our own Qt we also need to build QtKeychain with it
# so that it doesn't pull a different Qt version. For that reason
# first look in the Qt lib directory for QtKeychain.
get_target_property(_QTCORE_LIB_PATH Qt5::Core IMPORTED_LOCATION_RELEASE)
get_filename_component(QT_LIB_DIR "${_QTCORE_LIB_PATH}" DIRECTORY)

find_path(QTKEYCHAIN_INCLUDE_DIR
            NAMES
              keychain.h
            HINTS
               ${QT_LIB_DIR}/../include
            PATH_SUFFIXES
              qt5keychain
            )

find_library(QTKEYCHAIN_LIBRARY
            NAMES
              qt5keychain
              lib5qtkeychain
            HINTS
               ${QT_LIB_DIR}
            PATHS
               /usr/lib
               /usr/lib/${CMAKE_ARCH_TRIPLET}
               /usr/local/lib
               /opt/local/lib
               ${CMAKE_LIBRARY_PATH}
               ${CMAKE_INSTALL_PREFIX}/lib
            )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set QTKEYCHAIN_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Qt5Keychain  DEFAULT_MSG
	QTKEYCHAIN_LIBRARY QTKEYCHAIN_INCLUDE_DIR)

mark_as_advanced(QTKEYCHAIN_INCLUDE_DIR QTKEYCHAIN_LIBRARY)
