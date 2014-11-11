
if(APPLE)
add_custom_target( mac_overlayplugin ALL 
                   xcodebuild -workspace ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloud.xcworkspace 
                            -scheme SyncStateFinder.osax SYMROOT=${CMAKE_CURRENT_BINARY_DIR} archive
                   COMMENT building Mac Overlay icons)

INSTALL( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/SyncStateFinder.osax/Contents
         DESTINATION ${CMAKE_INSTALL_PREFIX}/Library/ScriptingAdditions/SyncStateFinder.osax/ )
endif(APPLE)

