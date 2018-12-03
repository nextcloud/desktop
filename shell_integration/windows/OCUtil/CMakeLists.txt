add_library(OCUtil STATIC
    CommunicationSocket.cpp
    FileUtil.cpp
    RegistryUtil.cpp
    RemotePathChecker.cpp
    stdafx.cpp
    StringUtil.cpp
    OCUtil.rc
)

target_include_directories(OCUtil
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
