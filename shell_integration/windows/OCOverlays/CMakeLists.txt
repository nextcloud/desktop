add_library(OCOverlays MODULE
    DllMain.cpp
    OCOverlay.cpp
    OCOverlayFactory.cpp
    OCOverlayRegistrationHandler.cpp
    stdafx.cpp
    OCOverlay.rc
    OCOverlays.def
)

target_link_libraries(OCOverlays
    OCUtil)

install(TARGETS OCOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
