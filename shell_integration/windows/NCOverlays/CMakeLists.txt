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

set_target_properties(NCOverlays
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS NCOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_PDB_FILE:NCOverlays> DESTINATION bin OPTIONAL)
