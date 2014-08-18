
if(APPLE)
add_custom_target( mac_overlayplugin ALL 
                   xcodebuild -workspace ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/OwnCloud.xcworkspace 
                            -scheme OwnCloudFinder.osax SYMROOT=${CMAKE_CURRENT_BINARY_DIR} archive
                   COMMENT building Mac Overlay iccons)

INSTALL( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/OwnCloudFinder.osax/Contents
         DESTINATION ${CMAKE_INSTALL_PREFIX}/Library/ScriptingAdditions/OwnCloudFinder.osax/ )
endif(APPLE)

