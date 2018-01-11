add_library(OCContextMenu MODULE
    dllmain.cpp
    OCClientInterface.cpp
    OCContextMenu.cpp
    OCContextMenuFactory.cpp
    OCContextMenuRegHandler.cpp
    stdafx.cpp
)

target_link_libraries(OCContextMenu
    OCUtil)

install(TARGETS OCContextMenu
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
