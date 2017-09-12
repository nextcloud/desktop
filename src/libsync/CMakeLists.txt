project(libsync)
set(CMAKE_AUTOMOC TRUE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
# csync is required.
include_directories(${CMAKE_SOURCE_DIR}/src/csync
                    ${CMAKE_BINARY_DIR}/src/csync
                   )

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

if(SPARKLE_FOUND AND NOT BUILD_LIBRARIES_ONLY)
    list (APPEND OS_SPECIFIC_LINK_LIBRARIES ${SPARKLE_LIBRARY})
endif()

set(libsync_SRCS
    account.cpp
    bandwidthmanager.cpp
    capabilities.cpp
    clientproxy.cpp
    connectionvalidator.cpp
    cookiejar.cpp
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
    propagatedownload.cpp
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    propagateremotedelete.cpp
    propagateremotemove.cpp
    propagateremotemkdir.cpp
    syncengine.cpp
    syncfilestatus.cpp
    syncfilestatustracker.cpp
    syncjournaldb.cpp
    syncjournalfilerecord.cpp
    syncresult.cpp
    theme.cpp
    ownsql.cpp
    checksums.cpp
    excludedfiles.cpp
    clientsideencryption.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialscommon.cpp
)

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS ${libsync_SRCS} creds/tokencredentials.cpp)
else()
    set (libsync_SRCS ${libsync_SRCS} creds/httpcredentials.cpp)
endif()

# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    account.h
    syncengine.h
    configfile.h
    networkjobs.h
    progressdispatcher.h
    syncfileitem.h
    syncjournaldb.h
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

list(APPEND libsync_LINK_TARGETS
    ${QT_LIBRARIES}
    ocsync
    OpenSSL::Crypto
    OpenSSL::SSL
    ${OS_SPECIFIC_LINK_LIBRARIES}
)

if(QTKEYCHAIN_FOUND OR QT5KEYCHAIN_FOUND)
    list(APPEND libsync_LINK_TARGETS ${QTKEYCHAIN_LIBRARY})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

if(INOTIFY_FOUND)
    include_directories(${INOTIFY_INCLUDE_DIR})
    link_directories(${INOTIFY_LIBRARY_DIR})
endif()

if(ZLIB_FOUND)
    list(APPEND libsync_LINK_TARGETS ${ZLIB_LIBRARIES})
    include_directories(${ZLIB_INCLUDE_DIRS})
endif(ZLIB_FOUND)

add_library(${synclib_NAME} SHARED ${libsync_SRCS} ${syncMoc})
GENERATE_EXPORT_HEADER( ${synclib_NAME}
	BASE_NAME ${synclib_NAME}
	EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
	STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

if(TOKEN_AUTH_ONLY)
    qt5_use_modules(${synclib_NAME} Network Concurrent)
else()
    qt5_use_modules(${synclib_NAME} Widgets Network Concurrent)
endif()

set_target_properties( ${synclib_NAME}  PROPERTIES
	VERSION ${MIRALL_VERSION}
	SOVERSION ${MIRALL_SOVERSION}
	RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)
set_target_properties( ${synclib_NAME} PROPERTIES
	INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

target_link_libraries(${synclib_NAME} ${libsync_LINK_TARGETS} )

if(INOTIFY_FOUND)
    target_link_libraries(${synclib_NAME} ${INOTIFY_LIBRARY} )
endif()

if(BUILD_LIBRARIES_ONLY)
    #add_library(${synclib_NAME}_static STATIC ${libsync_SRCS} ${syncMoc})
    #qt5_use_modules(${synclib_NAME}_static Widgets Network Xml Sql)

    #set_target_properties( ${synclib_NAME}_static  PROPERTIES
    #    VERSION ${MIRALL_VERSION}
    #    SOVERSION ${MIRALL_SOVERSION}
    #)

    #target_link_libraries(${synclib_NAME}_static ${libsync_LINK_TARGETS} )
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS ${synclib_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS ${synclib_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
    if (SPARKLE_FOUND)
        install(DIRECTORY "${SPARKLE_LIBRARY}"
	        DESTINATION "${OWNCLOUD_OSX_BUNDLE}/Contents/Frameworks" USE_SOURCE_PERMISSIONS)
    endif (SPARKLE_FOUND)
endif()


