
# Check if varialbe MAC_INSTALLER_BACKGROUND_FILE is defined. That might come
# from the OEM.cmake for branded clients or from OWNCLOUD.cmake for the non
# branded client.
# Make sure that the MAC_INSTALLER_BACKGROUND_FILE contains the full path, ie.
# includes CMAKE_SOURCE_DIR or so.

if (DEFINED MAC_INSTALLER_BACKGROUND_FILE )
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "1")
else()
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "0")
endif()

configure_file(create_mac_pkg.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/create_mac.sh)
configure_file(macosx.pkgproj ${CMAKE_CURRENT_BINARY_DIR}/macosx.pkgproj)
