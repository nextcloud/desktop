add_library(NCOverlays MODULE
    DllMain.cpp
    NCOverlay.cpp
    NCOverlayFactory.cpp
    NCOverlayRegistrationHandler.cpp
    NCOverlay.rc
    NCOverlays.def
)

target_link_libraries(NCOverlays
    NCUtil)

install(TARGETS NCOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
