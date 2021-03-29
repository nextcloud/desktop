include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)
configure_file(WinShellExtConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/WinShellExtConstants.h)

add_subdirectory(NCContextMenu)
add_subdirectory(NCOverlays)
add_subdirectory(NCUtil)

if(BUILD_WIN_MSI)
    configure_file(WinShellExt.wxs.in ${CMAKE_CURRENT_BINARY_DIR}/WinShellExt.wxs)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/WinShellExt.wxs
        DESTINATION msi/
    )
endif()
