# SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: LGPL-2.1-or-later
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
        VERSION
            ${WINDOWS_INTEGRATION_VERSION}
        SOVERSION
            ${WINDOWS_INTEGRATION_SOVERSION}
)

install(TARGETS NCOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_PDB_FILE:NCOverlays> DESTINATION bin OPTIONAL)
