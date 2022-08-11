set(LibreGraphAPIVersion v0.13.2)
# TODO: pass version to find_package once LibreGraphAPI provides versioned releases
find_package(LibreGraphAPI QUIET)
if (NOT TARGET OpenAPI::LibreGraphAPI)
    message(WARNING "Failed to find system OpenAPI::LibreGraphAPI: fetching ${LibreGraphAPIVersion}")
    include(FetchContent)
    FetchContent_Populate(LibreGraphAPISrc
                        QUIET
                        GIT_REPOSITORY https://github.com/owncloud/libre-graph-api-cpp-qt-client.git
                        GIT_TAG ${LibreGraphAPIVersion})
    add_subdirectory(${libregraphapisrc_SOURCE_DIR}/client ${libregraphapisrc_BINARY_DIR}/client EXCLUDE_FROM_ALL)
endif()

set(libsync_SRCS
    account.cpp
    bandwidthmanager.cpp
    capabilities.cpp
    cookiejar.cpp
    discovery.cpp
    discoveryphase.cpp
    filesystem.cpp
    httplogger.cpp
    jobqueue.cpp
    logger.cpp
    accessmanager.cpp
    configfile.cpp
    abstractnetworkjob.cpp
    networkjobs.cpp
    owncloudpropagator.cpp
    owncloudtheme.cpp
    progressdispatcher.cpp
    propagatorjobs.cpp
    propagatedownload.cpp
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    propagateuploadtus.cpp
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
    creds/credentialmanager.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialscommon.cpp
    creds/oauth.cpp
    creds/webfinger.cpp

    networkjobs/fetchuserinfojobfactory.cpp
    networkjobs/checkserverjobfactory.cpp
    networkjobs/jsonjob.cpp

    abstractcorejob.cpp
    determineauthtypejobfactory.cpp

    graphapi/drives.cpp
)

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS ${libsync_SRCS} creds/tokencredentials.cpp)
else()
    set(libsync_SRCS ${libsync_SRCS} creds/httpcredentials.cpp)
endif()

# These headers are installed for libowncloudsync to be used by 3rd party apps
INSTALL(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/owncloudlib.h
        logger.h
        accessmanager.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/ownCloud/libsync
)

add_library(libsync SHARED ${libsync_SRCS})
set_target_properties(libsync PROPERTIES EXPORT_NAME SyncCore)

target_link_libraries(libsync
    PUBLIC
        csync
        Qt5::Core
        Qt5::Network
    PRIVATE
        Qt5::Concurrent
        ZLIB::ZLIB
)

apply_common_target_settings(libsync)

target_link_libraries(libsync PUBLIC $<BUILD_INTERFACE:OpenAPI::LibreGraphAPI>)
add_subdirectory(graphapi)

if ( APPLE )
    target_link_libraries(libsync PUBLIC
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    target_link_libraries(libsync
        PUBLIC
            Qt5::Widgets
        PRIVATE
            qt5keychain)
endif()

if(Inotify_FOUND)
    target_include_directories(libsync PRIVATE ${Inotify_INCLUDE_DIRS})
    target_link_libraries(libsync PUBLIC ${Inotify_LIBRARIES})
endif()

if(NO_MSG_HANDLER)
    target_compile_definitions(libsync PRIVATE -DNO_MSG_HANDLER=1)
endif()

GENERATE_EXPORT_HEADER(libsync
        EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
        STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

set_target_properties(libsync PROPERTIES
        OUTPUT_NAME "${synclib_NAME}"
        VERSION ${MIRALL_VERSION}
        SOVERSION ${MIRALL_SOVERSION}
)
install(TARGETS libsync EXPORT ownCloudConfig ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
