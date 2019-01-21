project(libsync)
set(CMAKE_AUTOMOC TRUE)


if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

IF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|NetBSD|OpenBSD")
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
	inotify
    )
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|NetBSD|OpenBSD")

set(libsync_SRCS
    account.cpp
    bandwidthmanager.cpp
    capabilities.cpp
    cookiejar.cpp
    discovery.cpp
    discoveryphase.cpp
    filesystem.cpp
    logger.cpp
    accessmanager.cpp
    configfile.cpp
    abstractnetworkjob.cpp
    networkjobs.cpp
    owncloudpropagator.cpp
    owncloudtheme.cpp
    progressdispatcher.cpp
    propagatorjobs.cpp
    propagatecommonzsync.cpp
    propagatedownload.cpp
    propagatedownloadzsync.cpp
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    propagateremotedelete.cpp
    propagateremotemove.cpp
    propagateremotemkdir.cpp
    syncengine.cpp
    syncfileitem.cpp
    syncfilestatustracker.cpp
    localdiscoverytracker.cpp
    syncresult.cpp
    theme.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialscommon.cpp
)

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS ${libsync_SRCS} creds/tokencredentials.cpp)
else()
    set (libsync_SRCS ${libsync_SRCS} creds/httpcredentials.cpp)
endif()

## begin zsync

include_directories( ${CMAKE_SOURCE_DIR}/src/3rdparty/zsync/c )

set( libsync_SRCS
    ${libsync_SRCS}
    ../3rdparty/zsync/c/librcksum/hash.c
    ../3rdparty/zsync/c/librcksum/md4.c
    ../3rdparty/zsync/c/librcksum/range.c
    ../3rdparty/zsync/c/librcksum/rsum.c
    ../3rdparty/zsync/c/librcksum/state.c
    ../3rdparty/zsync/c/libzsync/sha1.c
    ../3rdparty/zsync/c/libzsync/zsync.c
    ../3rdparty/zsync/c/libzsync/zsyncfile.c
    ../3rdparty/zsync/c/progress.c
)

if ( WIN32 )
    # ntohs
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
        ws2_32
    )
    # ensure size_t is 64 bits
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
endif()

set_source_files_properties( ../3rdparty/zsync/c/libzsync/zsync.c
    PROPERTIES COMPILE_FLAGS -DVERSION=\\"0.6.3\\" )
set_source_files_properties( ../3rdparty/zsync/c/libzsync/zsyncfile.c
    PROPERTIES COMPILE_FLAGS "-DVERSION=\\\"0.6.3\\\" -DSIZEOF_OFF_T=8" )

## end zsync

# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    account.h
    syncengine.h
    configfile.h
    networkjobs.h
    progressdispatcher.h
    syncfileitem.h
    syncresult.h
)

set(creds_HEADERS
    creds/abstractcredentials.h
    creds/httpcredentials.h
)

IF (NOT APPLE)
    INSTALL(
        FILES ${owncloudsync_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/mirall
    )
    INSTALL(
        FILES ${creds_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/creds
    )
ENDIF(NOT APPLE)

add_library(${synclib_NAME} SHARED ${libsync_SRCS})
target_link_libraries(${synclib_NAME}
    "${csync_NAME}"
    ${OS_SPECIFIC_LINK_LIBRARIES}
    Qt5::Core Qt5::Network
)

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    target_link_libraries(${synclib_NAME} Qt5::Widgets ${QTKEYCHAIN_LIBRARY})
    target_include_directories(${synclib_NAME} PRIVATE ${QTKEYCHAIN_INCLUDE_DIR})
endif()

if(INOTIFY_FOUND)
    target_include_directories(${synclib_NAME} PRIVATE ${INOTIFY_INCLUDE_DIR})
    link_directories(${INOTIFY_LIBRARY_DIR})
    target_link_libraries(${synclib_NAME} ${INOTIFY_LIBRARY} )
endif()

GENERATE_EXPORT_HEADER( ${synclib_NAME}
	BASE_NAME ${synclib_NAME}
	EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
	STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

target_include_directories(${synclib_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})


set_target_properties( ${synclib_NAME}  PROPERTIES
	VERSION ${MIRALL_VERSION}
	SOVERSION ${MIRALL_SOVERSION}
	RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS ${synclib_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS ${synclib_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()


add_subdirectory(vfs)
