
FIND_LIBRARY(CSYNC_LIBRARY NAMES csync 
	HINTS "../buildcsync/src" )
FIND_PATH(CSYNC_INCLUDE_PATH csync.h
    HINTS "../csync/src" "/usr/include/csync")

SET(CSYNC_INCLUDE_DIR ${CSYNC_INCLUDE_PATH})

# handle the QUIETLY and REQUIRED arguments and set CSYNC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Csync DEFAULT_MSG
  CSYNC_LIBRARY CSYNC_INCLUDE_PATH)

MARK_AS_ADVANCED(
  CSYNC_INCLUDE_PATH
  CSYNC_LIBRARY)
