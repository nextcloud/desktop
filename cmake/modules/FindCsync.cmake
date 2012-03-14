IF(EXISTS "${CMAKE_SOURCE_DIR}/../csync/src/csync.h")
  SET(CSYNC_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/../csync/src/)
ELSE(EXISTS "${CMAKE_SOURCE_DIR}/../csync/src/csync.h")
  FIND_PATH(CSYNC_INCLUDE_PATH NAMES csync.h HINTS /usr/include/csync)
ENDIF(EXISTS "${CMAKE_SOURCE_DIR}/../csync/src/csync.h")

IF(EXISTS "${CMAKE_SOURCE_DIR}/../buildcsync/src/")
  IF( WIN32 )
     SET(CSYNC_LIBRARY ${CMAKE_SOURCE_DIR}/../win/buildcsync/src/libcsync.dll)
  ELSE( WIN32 )
     SET(CSYNC_LIBRARY ${CMAKE_SOURCE_DIR}/../buildcsync/src/libcsync.so)
  ENDIF( WIN32 )
ELSE(EXISTS "${CMAKE_SOURCE_DIR}/../buildcsync/src/")
  FIND_LIBRARY(CSYNC_LIBRARY NAMES csync )
ENDIF(EXISTS "${CMAKE_SOURCE_DIR}/../buildcsync/src/")

SET(CSYNC_INCLUDE_DIR ${CSYNC_INCLUDE_PATH})

# handle the QUIETLY and REQUIRED arguments and set CSYNC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Csync DEFAULT_MSG
  CSYNC_LIBRARY CSYNC_INCLUDE_PATH)

MARK_AS_ADVANCED( CSYNC_INCLUDE_PATH CSYNC_LIBRARY )
