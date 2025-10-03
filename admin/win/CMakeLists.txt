# MSI package scripts, helper DLL and migration tools
if(BUILD_WIN_MSI)
    add_subdirectory(msi)
endif()

if(BUILD_WIN_MSI OR BUILD_WIN_TOOLS)
    add_subdirectory(tools)
endif()
