add_library(OCContextMenu MODULE
    dllmain.cpp
    OCClientInterface.cpp
    OCContextMenu.cpp
    OCContextMenuFactory.cpp
    OCContextMenuRegHandler.cpp
    OCContextMenu.rc
    OCContextMenu.def
)

target_link_libraries(OCContextMenu
                      OCUtil
                      crypt32
                      gdiplus)
target_compile_definitions(OCContextMenu PRIVATE JSON_NOEXCEPTION)

install(TARGETS OCContextMenu
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
