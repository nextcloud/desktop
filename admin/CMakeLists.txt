if(APPLE)
    # traverse into osx subdirectory to install and patch the create-pack script
    add_subdirectory(osx)
elseif(WIN32)
    # MSI package scripts, helper DLL and migration tools
    add_subdirectory(win)
endif()
