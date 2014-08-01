
# Install the Mac icon container into the Mac Bundle.
if( BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY )
	set (ICON_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/icons)
	
	file(GLOB mac_icons "icns/*icns")
	install(FILES ${mac_icons} DESTINATION ${ICON_DIR})
endif()

