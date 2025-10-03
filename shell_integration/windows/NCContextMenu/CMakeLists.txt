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

install(TARGETS NCContextMenu
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
