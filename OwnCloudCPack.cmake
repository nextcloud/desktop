INCLUDE( InstallRequiredSystemLibraries )

SET( CPACK_PACKAGE_CONTACT  "Dominik Schmidt <domme@tomahawk-player.org>" )

SET( APPLICATION_NAME "ownCloud Client")

SET( CPACK_PACKAGE_FILE_NAME  owncloud-setup )    # Package file name without extension. Also a directory of installer  cmake-2.5.0-Linux-i686

# CPACK_GENERATOR   CPack generator to be used  STGZ;TGZ;TZ
# CPACK_INCLUDE_TOPLEVEL_DIRECTORY    Controls whether CPack adds a top-level directory, usually of the form ProjectName-Version-OS, to the top of package tree.  0 to disable, 1 to enable
# CPACK_INSTALL_CMAKE_PROJECTS    List of four values: Build directory, Project Name, Project Component, Directory in the package     /home/andy/vtk/CMake-bin;CMake;ALL;/
SET( CPACK_PACKAGE_DESCRIPTION_FILE  "${CMAKE_SOURCE_DIR}/README.md" ) # File used as a description of a project     /path/to/project/ReadMe.txt
SET( CPACK_PACKAGE_DESCRIPTION_SUMMARY  "ownCloud Syncing Client" ) #  Description summary of a project
# CPACK_PACKAGE_EXECUTABLES   List of pairs of executables and labels. Used by the NSIS generator to create Start Menu shortcuts.     ccmake;CMake
SET( CPACK_PACKAGE_INSTALL_DIRECTORY  ${APPLICATION_NAME} )     # Installation directory on the target system -> C:\Program Files\fellody
SET( CPACK_PACKAGE_INSTALL_REGISTRY_KEY ${APPLICATION_NAME} )  # Registry key used when installing this project  CMake 2.5.0
SET( CPACK_PACKAGE_NAME  ${APPLICATION_NAME} ) # Package name, defaults to the project name
SET( CPACK_PACKAGE_VENDOR  "http://owncloud.com" )   # Package vendor name

SET( CPACK_PACKAGE_VERSION_MAJOR  1 )
SET( CPACK_PACKAGE_VERSION_MINOR  0 )
SET( CPACK_PACKAGE_VERSION_PATCH  0 )
SET( CPACK_PACKAGE_VERSION  ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH} )

# SET( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt" ) # License file for the project, used by the STGZ, NSIS, and PackageMaker generators.  /home/andy/vtk/CMake/Copyright.txt


SET( CPACK_TOPLEVEL_TAG "unused" ) # Directory for the installed files.  - needed to provide anything to avoid an error# CPACK_INSTALL_COMMANDS  Extra commands to install components.


# Set the options file that needs to be included inside CMakeCPackOptions.cmake
#SET(QT_DIALOG_CPACK_OPTIONS_FILE ${CMake_BINARY_DIR}/Source/QtDialog/QtDialogCPack.cmake)
configure_file("${CMAKE_SOURCE_DIR}/CPackOptions.cmake.in"
    "${CMAKE_BINARY_DIR}/CPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackOptions.cmake") # File included at cpack time, once per generator after setting CPACK_GENERATOR to the actual generator being used; allows per-generator setting of CPACK_* variables at cpack time.  ${PROJECT_BINARY_DIR}/CPackOptions.cmake
include(CPack)
