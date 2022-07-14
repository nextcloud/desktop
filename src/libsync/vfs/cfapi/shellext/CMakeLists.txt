add_library(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} SHARED
    dllmain.cpp
    cfapishellintegrationclassfactory.cpp
    thumbnailprovider.cpp
	thumbnailprovideripc.cpp
    CfApiShellIntegration.def
)

target_link_libraries(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} shlwapi Gdiplus Qt5::Core Qt5::Network)

target_include_directories(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE ${GeneratedFilesPath})

target_include_directories(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS ${CFAPI_SHELL_EXTENSIONS_LIB_NAME} 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
