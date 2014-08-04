
# Install the Mac icon container into the Mac Bundle.
if( BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY )
	set (ICON_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/icons)
	
	file(GLOB mac_icons "icns/*icns")
	install(FILES ${mac_icons} DESTINATION ${ICON_DIR})
endif()

if( UNIX AND NOT APPLE )

    SET(ICON_DIR ${DATADIR}/icons/hicolor)
    
    FOREACH(size 128x128 16x16 256x256 32x32 48x48 64x64 72x72)
	install(DIRECTORY ${size} DESTINATION ${ICON_DIR}/${size}/apps FILES_MATCHING PATTERN "*.png")
    ENDFOREACH(size)

endif()



