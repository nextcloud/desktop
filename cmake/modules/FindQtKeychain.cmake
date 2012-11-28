# - Try to find QtKeyChain
# Once done this will define
#  QTKEYCHAIN_FOUND - System has QtKeyChain
#  QTKEYCHAIN_INCLUDE_DIRS - The QtKeyChain include directories
#  QTKEYCHAIN_LIBRARIES - The libraries needed to use QtKeyChain
#  QTKEYCHAIN_DEFINITIONS - Compiler switches required for using LibXml2

find_path(QTKEYCHAIN_INCLUDE_DIR qtkeychain/keychain.h)

find_library(QTKEYCHAIN_LIBRARY NAMES libqtkeychain qtkeychain)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set QTKEYCHAIN_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(QtKeyChain  DEFAULT_MSG
	QTKEYCHAIN_LIBRARY QTKEYCHAIN_INCLUDE_DIR)

mark_as_advanced(QTKEYCHAIN_INCLUDE_DIR QTKEYCHAIN_LIBRARY )
