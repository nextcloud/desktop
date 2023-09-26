
# Check if variable MAC_INSTALLER_BACKGROUND_FILE is defined.
# Make sure that the MAC_INSTALLER_BACKGROUND_FILE contains the full path, ie.
# includes CMAKE_SOURCE_DIR or so.

if (DEFINED MAC_INSTALLER_BACKGROUND_FILE )
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "1")
else()
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "0")
endif()

find_package(Qt${QT_VERSION_MAJOR} ${REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)
configure_file(create_mac.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/create_mac.sh)
configure_file(macosx.pkgproj.cmake ${CMAKE_CURRENT_BINARY_DIR}/macosx.pkgproj)
configure_file(pre_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/pre_install.sh)
configure_file(post_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/post_install.sh)
configure_file(QtWebEngineProcess.entitlements ${CMAKE_CURRENT_BINARY_DIR}/QtWebEngineProcess.entitlements)
