# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: LGPL-2.1-or-later
add_library(NCContextMenu MODULE
    dllmain.cpp
    NCClientInterface.cpp
    NCContextMenu.cpp
    NCContextMenuFactory.cpp
    NCContextMenuRegHandler.cpp
    NCContextMenu.rc
    NCContextMenu.def
)

target_link_libraries(NCContextMenu
    NCUtil)

set_target_properties(NCContextMenu
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

install(TARGETS NCContextMenu
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_PDB_FILE:NCContextMenu> DESTINATION bin OPTIONAL)
