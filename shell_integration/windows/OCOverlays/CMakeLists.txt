add_library(OCOverlays MODULE
    DllMain.cpp
    OCOverlay.cpp
    OCOverlayFactory.cpp
    OCOverlayRegistrationHandler.cpp
    OCOverlay.rc
    OCOverlays.def
)

target_link_libraries(OCOverlays
    OCUtil)

add_windows_version_info(OCOverlays)

install(TARGETS OCOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
