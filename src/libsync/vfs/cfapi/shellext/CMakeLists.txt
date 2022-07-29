add_library(CfApiShellExtensions MODULE
    dllmain.cpp
    cfapishellintegrationclassfactory.cpp
    thumbnailprovider.cpp
	thumbnailprovideripc.cpp
    ${CMAKE_SOURCE_DIR}/src/common/shellextensionutils.cpp
    CfApiShellIntegration.def
)

target_link_libraries(CfApiShellExtensions shlwapi Gdiplus Nextcloud::csync Qt5::Core Qt5::Network)

target_include_directories(CfApiShellExtensions PRIVATE ${GeneratedFilesPath})

target_include_directories(CfApiShellExtensions PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(CfApiShellExtensions
    PROPERTIES
        LIBRARY_OUTPUT_NAME
            ${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
        RUNTIME_OUTPUT_NAME
            ${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
        LIBRARY_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS CfApiShellExtensions 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
