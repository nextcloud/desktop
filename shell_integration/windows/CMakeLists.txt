# Use static runtime for all subdirectories
foreach(buildType "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
    string(REPLACE "/MD" "/MT" "CMAKE_CXX_FLAGS${buildType}" "${CMAKE_CXX_FLAGS${buildType}}")
endforeach()

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)
configure_file(WinShellExtConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/WinShellExtConstants.h)
configure_file(WinShellExtConstants.wxi.in ${CMAKE_CURRENT_BINARY_DIR}/WinShellExtConstants.wxi)

add_subdirectory(NCContextMenu)
add_subdirectory(NCOverlays)
add_subdirectory(NCUtil)

