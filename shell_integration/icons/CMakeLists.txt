
# Install the Mac icon container into the Mac Bundle.
if( BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY )
	set (ICON_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/icons)
	
	file(GLOB mac_icons "icns/*icns")
	install(FILES ${mac_icons} DESTINATION ${ICON_DIR})
endif()

if( UNIX AND NOT APPLE )

    SET(ICON_DIR ${DATADIR}/icons/hicolor)
    
    FOREACH(size 128x128 16x16 256x256 32x32 48x48 64x64 72x72)
	file(GLOB files  "${size}/*.png")
	FOREACH( file ${files} )
	    # the GLOB returns a absolute path. Make it relative by replacing the current src dir by nothing
	    STRING(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/${size}/" "" shortFile ${file})
	    STRING(REPLACE "oC" ${APPLICATION_NAME} brandedName ${shortFile})
	    install(FILES ${file} DESTINATION ${ICON_DIR}/${size}/apps RENAME ${brandedName})
	ENDFOREACH(file)
    ENDFOREACH(size)

endif()



