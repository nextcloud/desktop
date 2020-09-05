# Use static runtime for all subdirectories
foreach(buildType "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
    string(REPLACE "/MD" "/MT" "CMAKE_CXX_FLAGS${buildType}" "${CMAKE_CXX_FLAGS${buildType}}")
endforeach()

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
