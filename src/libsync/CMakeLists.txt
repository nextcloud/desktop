if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

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
    syncoptions.cpp
    theme.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialscommon.cpp
    creds/oauth.cpp
)

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS ${libsync_SRCS} creds/tokencredentials.cpp)
else()
    set (libsync_SRCS ${libsync_SRCS} creds/httpcredentials.cpp)
endif()

## begin zsync

add_library(zsync STATIC ../3rdparty/zsync/c/librcksum/hash.c
                        ../3rdparty/zsync/c/librcksum/md4.c
                        ../3rdparty/zsync/c/librcksum/range.c
                        ../3rdparty/zsync/c/librcksum/rsum.c
                        ../3rdparty/zsync/c/librcksum/state.c
                        ../3rdparty/zsync/c/libzsync/sha1.c
                        ../3rdparty/zsync/c/libzsync/zsync.c
                        ../3rdparty/zsync/c/libzsync/zsyncfile.c
                        ../3rdparty/zsync/c/progress.c
)

set_target_properties(zsync PROPERTIES C_STANDARD 11 AUTOMOC OFF)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(zsync PRIVATE -Wno-unused-parameter -Wno-unused-function)
endif()
target_include_directories(zsync PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/3rdparty/zsync/c>)

if ( WIN32 )
    target_link_libraries(zsync PUBLIC ws2_32)
    # ensure size_t is 64 bits
    target_compile_definitions(zsync PRIVATE _FILE_OFFSET_BITS=64)
endif()

target_compile_definitions(zsync PRIVATE VERSION="0.6.3" SIZEOF_OFF_T=8)

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
target_link_libraries(${synclib_NAME} PUBLIC
    "${csync_NAME}"
    ${OS_SPECIFIC_LINK_LIBRARIES}
    Qt5::Core Qt5::Network
    zsync
)

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    target_link_libraries(${synclib_NAME} PUBLIC Qt5::Widgets qt5keychain)
endif()

if(Inotify_FOUND)
    target_include_directories(${synclib_NAME} PRIVATE ${Inotify_INCLUDE_DIRS})
    target_link_libraries(${synclib_NAME} PUBLIC ${Inotify_LIBRARIES} )
endif()

GENERATE_EXPORT_HEADER( ${synclib_NAME}
        BASE_NAME ${synclib_NAME}
        EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
        STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

set_target_properties( ${synclib_NAME}  PROPERTIES
        VERSION ${MIRALL_VERSION}
        SOVERSION ${MIRALL_SOVERSION}
)
install(TARGETS ${synclib_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})


add_subdirectory(vfs)
