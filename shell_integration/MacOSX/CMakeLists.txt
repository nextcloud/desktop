
if(APPLE)
add_custom_target( mac_overlayplugin ALL 
                   xcodebuild -workspace ${CMAKE_SOURCE_DIR}/shell_integration/MacOSX/LiferayNativity.xcworkspace 
                            -scheme LiferayNativity.osax SYMROOT=${CMAKE_CURRENT_BINARY_DIR} archive 
                   COMMENT building Mac Overlay iccons)

INSTALL( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Release/LiferayNativity.osax/Contents
         DESTINATION ${CMAKE_INSTALL_PREFIX}/Library/ScriptingAdditions/LiferayNativity.osax/ )
endif(APPLE)

